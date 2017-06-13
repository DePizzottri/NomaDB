IF NOT EXIST "C:/lib/%VS_SHORT_NAME%/actor-framework/build/lib" (
  git clone https://github.com/DePizzottri/actor-framework.git
  cd actor-framework
  mkdir build
  cd build
  cmake %CMAKE_GENERATOR% -DCMAKE_GENERATOR_PLATFORM=%PLATFORM% -DCAF_NO_BENCHMARKS:BOOL=yes -DCAF_NO_EXAMPLES:BOOL=yes -DCAF_NO_MEM_MANAGEMENT:BOOL=yes -DCAF_NO_IO:BOOL=yes -DCAF_NO_UNIT_TESTS:BOOL=yes -DCAF_NO_OPENCL:BOOL=yes ..
  msbuild CAF.sln /p:Configuration=%CONFIGURATION% /p:Platform=%PLATFORM% /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"
  cd ../..
)
echo "Done building CAF"