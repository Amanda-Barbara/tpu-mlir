#!/bin/bash
set -e

source envsetup.sh
rm -rf ${INSTALL_PATH}
rm -rf ${PROJECT_ROOT}/regression/regression_out
source build.sh RELEASE

mlir_version="$(grep MLIR_VERSION ${BUILD_PATH}/CMakeCache.txt | cut -d "=" -f2)"
release_archive="./tpu-mlir_${mlir_version}"

rm -rf ${release_archive}*
cp -rf ${INSTALL_PATH} ${release_archive}

cp -rf ${PROJECT_ROOT}/regression ${release_archive}
cp -f ${PROJECT_ROOT}/doc/tpu-mlir_quick_start_guide.pdf \
      ${release_archive}/docs/"TPU-MLIR快速入门指南.pdf"

# generate regression run.sh
# ------------------------------------------------------------------------------
regression_sh=${release_archive}/regression/run.sh
echo "Create regression run.sh" 1>&2
more > "${regression_sh}" <<'//MY_CODE_STREAM'
#!/bin/bash
set -ex
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

rm -rf regression_out
mkdir regression_out
pushd regression_out

# run basic regression
$DIR/basic/run.sh
# run model-zoo regression if model-zoo exists.
$DIR/scripts/run.sh
popd

# for test
mkdir -m 777 -p bmodels
pushd bmodels
../prepare_bmrttest.py ../regression_out
cp -f ../run_bmrttest.py ./run.py
popd
//MY_CODE_STREAM

# generate model-zoo run.sh
# ------------------------------------------------------------------------------
model_zoo_sh=${release_archive}/regression/scripts/run.sh
echo "Create model-zoo run.sh" 1>&2
more > "${model_zoo_sh}" <<'//MY_CODE_STREAM'
#!/bin/bash
set -ex

if [ ! -d ${MODEL_ZOO_PATH} ]; then
  echo "[Warning] model-zoo does not exist; Skip model-zoo tests."
  exit 0
fi

DIR="$( cd "$(dirname "$0")" ; pwd -P )"

$DIR/run_mobilenet_v2.sh
$DIR/run_resnet50_v2.sh
$DIR/run_vgg16-12.sh
$DIR/run_resnet34_ssd1200.sh
//MY_CODE_STREAM


# build a envsetup.sh
# ------------------------------------------------------------------------------
__envsetupfile=${release_archive}/envsetup.sh
rm -f __envsetupfile

echo "Create ${__envsetupfile}" 1>&2
more > "${__envsetupfile}" <<'//MY_CODE_STREAM'
#!/bin/bash
# set environment variable
TPUC_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

export PATH=${TPUC_ROOT}/bin:$PATH
export PATH=${TPUC_ROOT}/python/tools:$PATH
export PATH=${TPUC_ROOT}/python/utils:$PATH
export PATH=${TPUC_ROOT}/python/samples:$PATH
export LD_LIBRARY_PATH=$TPUC_ROOT/lib:$LD_LIBRARY_PATH
export PYTHONPATH=${TPUC_ROOT}/python:$PYTHONPATH
export MODEL_ZOO_PATH=${TPUC_ROOT}/../model-zoo
export REGRESSION_PATH=${TPUC_ROOT}/regression
//MY_CODE_STREAM

# generate readme.md
# ------------------------------------------------------------------------------
echo "Create readme.md" 1>&2
more > "${release_archive}/readme.md" <<'//MY_CODE_STREAM'
# For test

0. Setup

If you have this file, congratulation, you have finished the first step of
exploring the "TPU-MLIR" release SDK.

Before getting a start, you need to prepare some configuration.

- a. Get the tpuc_dev docker image.
  More about docker usage. please check:
  [installation-methods](https://docs.docker.com/engine/install/ubuntu/#installation-methods)
  [Manage Docker as a non-root user](https://docs.docker.com/engine/install/linux-postinstall/)
  `docker pull sophgo/tpuc_dev:v1.1`.

- b. (Optional, If you want to test more cases.) Clone the "model-zoo" repository
  to the same directory in which you unpack TPU-MLIR. If you have already cloned
  and synced with `model-zoo`, jump to step d.
  [How to use model-zoo](https://github.com/sophgo/model-zoo)
  `git clone --depth=1 https://github.com/sophgo/model-zoo`.

- c. (Optional, with step c) Get into the "model-zoo" folder and pull LFS files
  from the server.
  `git lfs pull --include "*.onnx" --exclude=""`

- d. Create a docker container and map the directory of "TPU-MLIR" to it.
  `docker run -v $PWD:/workspace/ -ti sophgo/tpuc_dev:v1.1 /bin/bash`

- e. If everything goes well, you can go to the next stage.

1. Set envrironment variables.

You need to set some system variables to ensure all the functions of TPU-MLIR
can work well. The commands below will make all the executable files provided by
TPU-MLIR visible to the system.

Get into the tpu-mlir_v*** directory, then run the command below.

``` bash
source ./envsetup.sh
```

2. Run some test

This step will run lots of test cases to demonstrate all the features of
TPU-MLIR, which include:

- a. Translate ONNX/TFLite models to framework-independent TOP MLIR.

- b. (Optional) Do calibration, and tranform float computes model to integer computations
  which are friendly to the hardware(BM1684X).

- c. Transform TOP MLIR to TPU MLIR, which is close to the hardware.

- d. Generate BM1684X machine code (bmodel).

``` bash
cd regression
 ./run.sh
```

After run regression test, all the outputs will be in `bmodels`.

3. Test the performance of Bmodels on BM1684X.

TPU-MLIR does not provide "bmrt_test" excutable file. It would be best to have
an environment where "bmrt_test" is available.

- If your BM1684X devide runs in PCIE mode, use the commands below.

Assume you have set up the host PC correctly, which means the `shell` can call
"bmrt_test," go out of the docker container and enter the bmodels folder
generated by the previous step. Then run the command below.

``` bash
./run.py
```

A "*.csv" file(report) will be generated in this folder.

- If your BM1684X device runs in SOC mode, use the commands below.

``` bash
cp -rf bmodels /to/your/BM1684X/soc/board # eg. ~/
cd /to/bmodels # eg. ~/
./run.py
```

A "*.csv" file(report) will be generated in this folder.


//MY_CODE_STREAM
# ------------------------------------------------------------------------------


tar -cvzf "tpu-mlir_${mlir_version}.tar.gz" ${release_archive}
rm -rf ${release_archive}
