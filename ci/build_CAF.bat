IF NOT EXIST "C:/lib/actor-framework/build/lib" (
  git clone --branch 0.15.3 https://github.com/actor-framework/actor-framework.git
  cd actor-framework
  mkdir build
  cd build
  cmake %CMAKE_GENERATOR% -DCMAKE_GENERATOR_PLATFORM=%PLATFORM% -DCAF_NO_BENCHMARKS:BOOL=yes -DCAF_NO_EXAMPLES:BOOL=yes -DCAF_NO_MEM_MANAGEMENT:BOOL=yes -DCAF_NO_IO:BOOL=yes -DCAF_NO_UNIT_TESTS:BOOL=yes -DCAF_NO_OPENCL:BOOL=yes ..
  msbuild CAF.sln /p:Configuration=%CONFIGURATION% /p:Platform=%PLATFORM%
  cd ../..
)
echo "Done building CAF"