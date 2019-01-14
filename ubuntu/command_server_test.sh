git checkout headunit.json

echo "basic tests"
curl "127.0.0.1:9999/status"
curl "127.0.0.1:9999/updateConfig?parameter=callbackParamString&value=onestringparamtorulethemall&type=string"
curl "127.0.0.1:9999/updateConfig?parameter=callbackParamBool_setTo_False&value=false&type=bool"
curl "127.0.0.1:9999/updateConfig?parameter=callbackParamBool_setTo_True&value=true&type=bool"
curl "127.0.0.1:9999/updateConfig?parameter=callbackParamBool_setTo_WrongValue&value=wrongValue&type=bool"
curl "127.0.0.1:9999/updateConfig?parameter=callbackParamBool_setTo_WrongType&value=whatever&type=wrongType"
curl "127.0.0.1:9999/updateConfig?parameter=carGPS&value=false&type=bool"

echo "permission tests"
chmod -r headunit.json
curl "127.0.0.1:9999/updateConfig?parameter=callbackParamBool_cant_read&value=one&type=string"
chmod +r-w headunit.json
curl "127.0.0.1:9999/updateConfig?parameter=callbackParamBool_cant_write&value=one&type=string"
chmod +rw headunit.json

echo "no config file"
rm headunit.json
curl "127.0.0.1:9999/updateConfig?parameter=callbackParamBool_no_file&value=one&type=string"

echo "corrupt file"
echo " {launchOnDevice: notbool, carGPS: true}" > headunit.json
curl "127.0.0.1:9999/updateConfig?parameter=callbackParamBool_corrupt_file&value=one&type=string"

echo " {launchOnDevice: 12, carGPS: true}" > headunit.json
curl "127.0.0.1:9999/updateConfig?parameter=callbackParamBool_corrupt_file&value=one&type=string"
