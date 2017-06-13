IF NOT EXIST C:/lib/%VS_SHORT_NAME%/restbed/build/%CONFIGURATION% (
  git clone  --recursive --branch 4.5 https://github.com/Corvusoft/restbed.git
  cd restbed
  mkdir build
  cd build
  cmake -G %CMAKE_GENERATOR% -DCMAKE_GENERATOR_PLATFORM=%PLATFORM% -DBUILD_TESTS=NO -DBUILD_EXAMPLES=NO -DBUILD_SSL=NO -DBUILD_SHARED=NO ..
  msbuild restbed.sln /p:Configuration=%CONFIGURATION% /p:Platform=%PLATFORM% /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"
  cd ../..
)

echo "Done building restbed"
