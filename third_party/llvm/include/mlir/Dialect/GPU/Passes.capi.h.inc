
/* Autogenerated by mlir-tblgen; don't manually edit. */

#include "mlir-c/Pass.h"

#ifdef __cplusplus
extern "C" {
#endif

// Registration for the entire group
MLIR_CAPI_EXPORTED void mlirRegisterGPUPasses();


/* Create GPU Pass. */
MLIR_CAPI_EXPORTED MlirPass mlirCreateGPUGpuAsyncRegionPass();
MLIR_CAPI_EXPORTED void mlirRegisterGPUGpuAsyncRegionPass();


/* Create GPU Pass. */
MLIR_CAPI_EXPORTED MlirPass mlirCreateGPUGpuKernelOutlining();
MLIR_CAPI_EXPORTED void mlirRegisterGPUGpuKernelOutlining();


/* Create GPU Pass. */
MLIR_CAPI_EXPORTED MlirPass mlirCreateGPUGpuLaunchSinkIndexComputations();
MLIR_CAPI_EXPORTED void mlirRegisterGPUGpuLaunchSinkIndexComputations();



#ifdef __cplusplus
}
#endif