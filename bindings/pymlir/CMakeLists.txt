cmake_minimum_required(VERSION 2.8.12)

if (POLICY CMP0048)
  # cmake warns if loaded from a min-3.0-required parent dir, so silence the warning:
  cmake_policy(SET CMP0048 NEW)
endif()

project(pymlir)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
SET(CMAKE_INSTALL_RPATH "\${ORIGIN}/")


set(pybind11_DIR /share4algo02/e00130/workspace/project/BitMain/server/triton-inference-server-python_backend/cmake-build-debug/_deps/pybind11-src/build/share/cmake/pybind11)
find_package(pybind11 REQUIRED CONFIG)
pybind11_add_module(pymlir pymlir.cpp)

target_link_libraries(pymlir PRIVATE
  TPUMLIRInitAll
  MLIRTransforms
  MLIRParser
  LLVMCore
  LLVMSupport
)
install(TARGETS pymlir DESTINATION python)
