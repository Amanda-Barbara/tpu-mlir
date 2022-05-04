#include "GroupOps.h"
#include "sophgo/Support/MathUtils.h"
#include <numeric>

using namespace mlir;
using namespace sophgo::tpu;
using namespace sophgo::backend;

lmem_info_t *GroupOps::find_lmem_info(group_lmem_t group_lmem, mlir::Value v) {
  if (group_lmem == nullptr) {
    return nullptr;
  }
  for (auto &it : *group_lmem) {
    if (it.value == v) {
      return &it;
    }
  }
  return nullptr;
}
lmem_info_t *GroupOps::find_lmem_info(group_lmem_t group_lmem,
                                      mlir::Operation *op) {
  if (group_lmem == nullptr) {
    return nullptr;
  }
  for (auto &it : *group_lmem) {
    if (it.op == op) {
      return &it;
    }
  }
  return nullptr;
}

bool GroupOps::isWeightValue(Value v) {
  auto op = v.getDefiningOp();
  if (isa<top::WeightOp>(op)) {
    return true;
  }
  return false;
}

group_lmem_t GroupOps::list_lmems(int64_t start_idx, int64_t end_idx) {
  assert(end_idx > start_idx);
  int64_t id = 0;
  auto lmems = std::make_shared<std::vector<lmem_info_t>>();
  for (auto idx = start_idx; idx <= end_idx; idx++) {
    auto op = all_ops[idx];
    if (isa<top::WeightOp>(op)) {
      continue;
    }
    int64_t op_id = id + op->getNumOperands();
    for (auto opd : op->getOperands()) {
      auto in_op_ = opd.getDefiningOp();
      if (isa<top::NoneOp>(in_op_)) {
        continue;
      }
      if (isa<top::WeightOp>(in_op_)) {
        lmem_info_t li(LMEM_WEIGHT, id, op_id, op_id, opd, in_op_);
        lmems->emplace_back(li);
        id++;
        continue;
      }
      auto it = find_lmem_info(lmems, opd);
      if (it != nullptr) {
        it->end_id = op_id;
        continue;
      }

      lmem_info_t li(LMEM_TENSOR, id, op_id, op_id, opd, nullptr);
      li.is_input = true;
      lmems->emplace_back(li);
      id++;
    }
    lmem_info_t li(LMEM_OPERATION, op_id, op_id, op_id, nullptr, op);
    lmems->emplace_back(li);
    id = op_id + 1;
    auto out = op->getResult(0);
    lmem_info_t li_out(LMEM_TENSOR, id, op_id, op_id, out, op);
    lmems->emplace_back(li_out);
    id++;
  }
  // mark output
  for (auto &linfo : *lmems) {
    if (linfo.type != LMEM_TENSOR) {
      continue;
    }
    for (auto user : linfo.value.getUsers()) {
      if (find_lmem_info(lmems, user) == nullptr) {
        linfo.is_output = true;
        break;
      }
    }
    if (linfo.is_output) {
      linfo.end_id = -1; // resident in lmem
    }
  }
  return lmems;
}

GroupOps::GroupOps(::mlir::func::FuncOp func_) {
  int id = 1;
  func = func_;
  auto chip = Module::getChip(Module::getModuleOp(func.getOperation()));
  bm168x = BM168x::instance(chip);
  n_align = bm168x->get_n_align(1);
  func.walk([&](Operation *op) {
    if (isa<FuncOp, top::NoneOp, top::WeightOp>(op)) {
      // do nothing
    } else {
      all_ops.push_back(op);
    }
  });
}

bool GroupOps::isLgSupport(int64_t op_idx) {
  int64_t num_ops = all_ops.size();
  assert(op_idx < num_ops);
  auto op = all_ops[op_idx];
  if (isa<top::WeightOp>(op)) {
    return true;
  }
  auto lg_if = dyn_cast<sophgo::LayerGroupInterface>(op);
  if (!lg_if || mlir::failed(lg_if.Verify())) {
    return false;
  }
  return true;
}

void GroupOps::group_search() {
  int64_t num_ops = all_ops.size();
  int64_t start_idx = 0, end_idx = num_ops - 1;
  int64_t lmem_start = 0, lmem_end = 0;
  // from end to start search
  while (end_idx >= 0) {
    while (end_idx >= 0) {
      if (isLgSupport(end_idx) == true) {
        break;
      }
      end_idx++;
    }
    start_idx = end_idx;
    while (start_idx >= 0) {
      if (isLgSupport(start_idx) == false) {
        break;
      }
      start_idx++;
    }
    if (start_idx == end_idx) {
      end_idx -= 2;
      continue;
    }
    auto new_start_idx = start_idx;
    auto group_lmem = CreateGroup(start_idx, end_idx, new_start_idx);
    if (group_lmem != nullptr) {
      assert(new_start_idx >= start_idx);
      groups.push_back(group_pair_t(new_start_idx, end_idx));
      all_lmems.push_back(group_lmem);
      end_idx = new_start_idx - 1;
      continue;
    }
    end_idx -= 2;
  }
}

bool GroupOps::check_group(int64_t start_idx, int64_t end_idx) {
  if (start_idx >= end_idx) {
    return false;
  }
  std::set<Operation *> ops;
  for (auto i = start_idx; i <= end_idx; i++) {
    ops.insert(all_ops[i]);
  }
  for (auto op : ops) {
    // is all input
    bool is_all_input = true;
    for (auto opd : op->getOperands()) {
      auto in_ = opd.getDefiningOp();
      if (isa<top::NoneOp, top::WeightOp>(in_)) {
        continue;
      }
      if (ops.find(in_) != ops.end()) {
        is_all_input = false;
        break;
      }
    }
    if (is_all_input == false) {
      continue;
    }
    bool is_all_output = true;
    for (auto user : op->getUsers()) {
      if (ops.find(user) != ops.end()) {
        is_all_output = false;
        break;
      }
    }
    if (is_all_output == false) {
      continue;
    }
    return false;
  }
  return true;
}

void GroupOps::process() { group_search(); }

group_lmem_t GroupOps::CreateGroup(int64_t start_idx, int64_t end_idx,
                                   int64_t &new_start_idx) {
  if (check_group(start_idx, end_idx) == false) {
    return nullptr;
  }
  int nsecs = 1, hsecs = 1;
  // try no slice first
  new_start_idx = start_idx;
  while (new_start_idx > end_idx) {
    auto group_lmem = CreateGroupBySecs(new_start_idx, end_idx, nsecs, hsecs);
    if (group_lmem) {
      return group_lmem;
    }
    new_start_idx--;
  }
  auto end_op = all_ops[end_idx];
  auto out = end_op->getResult(0);
  int64_t n, c, h, w;
  Module::getNCHW(out, n, c, h, w);
  auto n_align = bm168x->get_n_align(1);
  int64_t max_nsecs = ceiling_func(n, n_align);
  int64_t max_hsecs = h;
  new_start_idx = start_idx;
  while (end_idx > new_start_idx) {
    no_more_try_secs = false;
    hsecs = 1;
    for (nsecs = 2; no_more_try_secs == false && nsecs <= max_nsecs; nsecs++) {
      auto group_lmem = CreateGroupBySecs(new_start_idx, end_idx, nsecs, hsecs);
      if (group_lmem) {
        return group_lmem;
      }
    }

    nsecs = max_nsecs;
    for (hsecs = 2; no_more_try_secs == false && hsecs <= max_hsecs; hsecs++) {
      auto group_lmem = CreateGroupBySecs(new_start_idx, end_idx, nsecs, hsecs);
      if (group_lmem) {
        return group_lmem;
      }
    }
    new_start_idx++;
  }
  return nullptr;
}

void GroupOps::slice_all_outputs(group_lmem_t group_lmem, int64_t nsecs,
                                 int64_t hsecs) {
  for (auto &linfo : *group_lmem) {
    if (linfo.is_output == false) {
      continue;
    }
    int64_t n, c, h, w;
    Module::getNCHW(linfo.value, n, c, h, w);
    slice_pair_t slice_pair;
    auto &si = linfo.slice_info;
    if (nsecs == 1) {
      si.n.emplace_back(slice_pair_t(0, n));
    } else {
      auto max_slice = align_up(ceiling_func(n, nsecs), n_align);
      auto offset = 0l;
      for (auto i = 0; i < nsecs; i++) {
        slice_pair.first = offset;
        slice_pair.second = std::min(max_slice, n - offset);
        si.n.emplace_back(slice_pair);
        offset += slice_pair.second;
      }
    }
    if (hsecs == 1) {
      si.h.emplace_back(slice_pair_t(0, h));
    } else {
      auto max_slice = ceiling_func(h, hsecs);
      auto offset = 0l;
      for (auto i = 0; i < hsecs; i++) {
        slice_pair.first = offset;
        slice_pair.second = std::min(max_slice, h - offset);
        si.h.emplace_back(slice_pair);
        offset += slice_pair.second;
      }
    }
    auto op_info = find_lmem_info(group_lmem, linfo.op);
    assert(op_info != nullptr);
    op_info->slice_info = si;
  }
}

void GroupOps::union_slice(slice_pair_t &target, slice_pair_t &from) {
  auto target_end = target.first + target.second;
  auto from_end = from.first + from.second;
  if (from.first > target.first) {
    target.first = from.first;
  }
  if (target_end < from_end) {
    target.second = from_end - target.first;
  } else {
    target.second = target_end - target.first;
  }
}

void GroupOps::union_slice(std::vector<slice_pair_t> &targets,
                           std::vector<slice_pair_t> &froms) {
  assert(targets.size() == froms.size());
  for (auto it : llvm::zip(targets, froms)) {
    union_slice(std::get<0>(it), std::get<1>(it));
  }
}

bool GroupOps::backward_from_tensor(group_lmem_t group_lmem, Value v) {
  auto linfo = find_lmem_info(group_lmem, v);
  assert(linfo != nullptr);
  assert(linfo->type == LMEM_TENSOR);
  auto &si = linfo->slice_info;
  assert(!si.n.empty());
  assert(!si.h.empty());
  // make sure all users ready
  for (auto user : v.getUsers()) {
    auto uinfo = find_lmem_info(group_lmem, user->getResult(0));
    if (uinfo == nullptr) {
      continue;
    }
    if (uinfo->slice_info.n.size() == 0) {
      return true;
    }
  }
  auto op = v.getDefiningOp();
  auto op_info = find_lmem_info(group_lmem, op);
  auto lg_op = cast<LayerGroupInterface>(op);
  std::vector<slice_pair_t> slice_n;
  std::vector<slice_pair_t> slice_h;
  for (auto &s : si.n) {
    int64_t n_idx = 0, n_slice = 0;
    auto ret = lg_op.BackwardN(n_idx, n_slice, s.first, s.second);
    if (failed(ret)) {
      return false;
    }
    slice_n.emplace_back(slice_pair_t(n_idx, n_slice));
  }
  for (auto &s : si.h) {
    int64_t h_idx = 0, h_slice = 0;
    auto ret = lg_op.BackwardH(h_idx, h_slice, s.first, s.second);
    if (failed(ret)) {
      return false;
    }
    slice_h.emplace_back(slice_pair_t(h_idx, h_slice));
  }
  for (auto opd : op->getOperands()) {
    auto in_info = find_lmem_info(group_lmem, opd);
    if (in_info->type != LMEM_TENSOR) {
      continue;
    }
    auto si = &in_info->slice_info;
    if (si->n.size() == 0) {
      si->n = slice_n;
      si->h = slice_h;
    } else {
      union_slice(si->n, slice_n);
      union_slice(si->h, slice_h);
    }
    if (false == check_hsecs(*in_info)) {
      no_more_try_secs = true;
      return false;
    }
    if (in_info->is_input) {
      continue;
    }
    auto ret = backward_from_tensor(group_lmem, opd);
    if (ret == false) {
      return false;
    }
  }
  return true;
}

bool GroupOps::backward_entry(group_lmem_t group_lmem) {
  for (auto &linfo : *group_lmem) {
    if (linfo.is_output == false) {
      continue;
    }
    auto ret = backward_from_tensor(group_lmem, linfo.value);
    if (ret == false) {
      return false;
    }
  }
  return true;
}

void get_max_slice_nh(const lmem_info_t &lmem_info, int64_t &max_n,
                      int64_t &max_h) {
  auto &si = lmem_info.slice_info;
  max_n = 0;
  max_h = 0;
  for (auto &slice : si.n) {
    if (slice.second > max_n) {
      max_n = slice.second;
    }
  }
  for (auto &slice : si.h) {
    if (slice.second > max_h) {
      max_h = slice.second;
    }
  }
}

void GroupOps::set_lmem_size(group_lmem_t group_lmem) {
  // set size
  int64_t n, c, h, w, slice_n, slice_h;
  for (auto &linfo : *group_lmem) {
    if (LMEM_WEIGHT == linfo.type) {
      linfo.size = bm168x->get_weight_lmem_bytes(linfo.value);
    } else if (LMEM_TENSOR == linfo.type) {
      get_max_slice_nh(linfo, slice_n, slice_h);
      linfo.size = bm168x->get_tensor_lmem_bytes(linfo.value, slice_n, slice_h);
    }
  }
  for (auto &linfo : *group_lmem) {
    if (LMEM_OPERATION == linfo.type) {
      auto lg_op = cast<LayerGroupInterface>(linfo.op);
      auto out = linfo.op->getResult(0);
      Module::getNCHW(out, n, c, h, w);
      auto out_info = find_lmem_info(group_lmem, out);
      assert(out_info != nullptr);
      get_max_slice_nh(*out_info, slice_n, slice_h);
      linfo.size = lg_op.getBufferSize(slice_n, c, slice_h, w, out_info->size);
    }
  }
}

group_lmem_t GroupOps::CreateGroupBySecs(int64_t start_idx, int64_t end_idx,
                                         int64_t nsecs, int64_t hsecs) {
  std::vector<Operation *> group_ops;
  for (auto i = start_idx; i <= end_idx; i++) {
    group_ops.push_back(all_ops[i]);
  }
  auto group_lmem = list_lmems(start_idx, end_idx);
  slice_all_outputs(group_lmem, nsecs, hsecs);
  auto ret = backward_entry(group_lmem);
  if (ret == false) {
    return nullptr;
  }
  // checkout all values have been sliced
  for (auto &linfo : *group_lmem) {
    if (linfo.type != LMEM_TENSOR) {
      continue;
    }
    auto si = linfo.slice_info;
    assert(nsecs == si.n.size());
    assert(hsecs == si.h.size());
  }
  // ajust weight timestep
  if (nsecs != 1 || hsecs != 1) {
    for (auto &linfo : *group_lmem) {
      if (linfo.type == LMEM_WEIGHT) {
        linfo.end_id = -1;
      }
    }
  }
  // update lmem size and addr
  set_lmem_size(group_lmem);
  ret = assign_lmem_addr(group_lmem, nsecs, hsecs);
  if (ret == false) {
    return nullptr;
  }
  return group_lmem;
}

lmem_info_t *find_max_unalloc_lmem(group_lmem_t group_lmem, int64_t op_id,
                                   lmem_type_t type) {
  int64_t size = 0;
  int64_t term = 0;
  lmem_info_t *p_li = nullptr;
  for (auto &linfo : *group_lmem) {
    if (linfo.addr != -1) {
      continue;
    }
    if (op_id >= 0) {
      if (linfo.start_id > op_id ||
          (linfo.end_id < op_id && linfo.end_id != -1)) {
        continue;
      }
    }
    if (type != LMEM_ANY) {
      if (type != linfo.type) {
        continue;
      }
    }
    auto term_ = linfo.end_id - linfo.start_id;
    if ((term_ < 0 && term >= 0) || ((term_ * term) >= 0 && term_ > term) ||
        (term_ == term && linfo.size > size)) {
      term = term_;
      size = linfo.size;
      p_li = &linfo;
    }
  }
  return p_li;
}

bool GroupOps::assign_lmem_addr(group_lmem_t group_lmem, int64_t nsecs,
                                int64_t hsecs) {
  int64_t start_addr = 0, end_addr = bm168x->get_lmem_bytes();
  if (nsecs != 1 || hsecs != 1) {
    // weight first
    lmem_info_t *p_li = nullptr;
    do {
      p_li = find_max_unalloc_lmem(group_lmem, -1, LMEM_WEIGHT);
      p_li->addr = alloc_lmem(p_li->size);
      if (p_li->addr < 0) {
        no_more_try_secs = true;
        return false;
      }
    } while (p_li != nullptr);
  }
  std::vector<int64_t> op_ids;
  for (auto &linfo : *group_lmem) {
    if (linfo.type == LMEM_OPERATION) {
      op_ids.push_back(linfo.id);
    }
  }
  for (auto op_id : op_ids) {
    free_unuse_lmem(group_lmem, op_id);
    lmem_info_t *p_li = nullptr;
    do {
      p_li = find_max_unalloc_lmem(group_lmem, op_id);
      if (p_li != nullptr) {
        p_li->addr = alloc_lmem(p_li->size);
        if (p_li->addr < 0) {
          return false;
        }
      }
    } while (p_li != nullptr);
  }
}

int64_t GroupOps::alloc_lmem(int64_t size) {
  bool in_bank[] = {true, false};
  if (size > bm168x->get_lmem_bytes()) {
    return -1;
  }
  for (auto align_bank : in_bank) {
    if (size > bm168x->get_lmem_bank_bytes()) {
      continue;
    }
    int64_t addr = 0;
    for (auto it = allocated_lmems.begin(); it != allocated_lmems.end(); ++it) {
      if (addr + size <= it->first) {
        auto pair = lmem_pair_t(addr, size);
        allocated_lmems.insert(it, pair);
        return addr;
      }
      addr = align_up(it->first + it->second, bm168x->get_eu_bytes());
      if (align_bank) {
        auto bank0 = ceiling_func(addr, bm168x->get_lmem_bank_bytes());
        auto bank1 =
            ceiling_func(addr + size - 1, bm168x->get_lmem_bank_bytes());
        if (bank0 != bank1) {
          addr = align_up(addr, bm168x->get_lmem_bank_bytes());
        }
      }
    }
    if (addr + size > bm168x->get_lmem_bytes()) {
      continue;
    }
    auto pair = lmem_pair_t(addr, size);
    allocated_lmems.push_back(pair);
    return addr;
  }
}

void GroupOps::free_lmem(int64_t addr) {
  for (auto it = allocated_lmems.begin(); it != allocated_lmems.end(); ++it) {
    if (it->first == addr) {
      allocated_lmems.erase(it);
      return;
    }
  }
  llvm_unreachable("can't find addr");
}

void GroupOps::free_unuse_lmem(group_lmem_t group_lmem, int64_t op_id) {
  for (auto &linfo : *group_lmem) {
    if (linfo.addr == -1 || linfo.end_id == -1) {
      continue;
    }
    if (linfo.start_id <= op_id && linfo.end_id >= op_id) {
      continue;
    }
    free_lmem(linfo.addr);
  }
}

bool GroupOps::check_hsecs(lmem_info_t &lmem_info) {
  assert(lmem_info.type == LMEM_TENSOR);
  auto &si_h = lmem_info.slice_info.h;
  assert(lmem_info.slice_info.h.size() > 0);
  int64_t n, c, h, w;
  Module::getNCHW(lmem_info.value, n, c, h, w);
  int64_t total_h = 0;
  for (auto &it : si_h) {
    total_h += it.second;
  }
  if (total_h * 2 > h * 3) { // h increase 1.5 times
    return false;
  }
  return true;
}