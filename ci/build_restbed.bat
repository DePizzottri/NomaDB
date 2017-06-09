IF NOT EXIST C:/lib/restbed/build/%CONFIGURATION% (
  git clone --branch 4.5 https://github.com/Corvusoft/restbed.git
  cd restbed
  mkdir build
  cd build
  cmake -G %CMAKE_GENERATOR% -DCMAKE_GENERATOR_PLATFORM=%PLATFORM% -DBUILD_TESTS=NO -DBUILD_EXAMPLES=NO -DBUILD_SSL=NO -DBUILD_SHARED=NO ..
  msbuild restbed.sln /p:Configuration=%CONFIGURATION% /p:Platform=%PLATFORM%
  cd ../..
)

echo "Done building restbed"
