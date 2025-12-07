local opcua = require('luaopcua')
local basexx = require('basexx')

-----------
-- Test extension object read/write encode/decode
--
local function main_test_extension_object(url, namespace, varname)

    -- create the opcua connection
    local client = opcua.Client.new()
    local config = client.config
    config:setTimeout(5000)
    config:setSecureChannelLifeTime(10 * 60 * 1000)

    local r, err = client:connect(url)   -- "opc.tcp://172.30.0.187:38133"
    print(r, err)
    print("StatusCodeName", opcua.getStatusCodeName(r))

    -- Prepare to connect to a structured data type (extension object)
    -- Use a (offline) "NodeId" object to query the server for a full "Node" object
    local varNodeId = opcua.NodeId.new(namespace, varname)  -- e.g. '"==300_LcsCom_he"."eventLogInOrOffUser"."request"'

    -- Ask the server to provide the full "Node" object for the variable (read "NodeId" attribute)
    local varNode = client:getNode(varNodeId)
    print('getNode ', varNode)                       -- the "node" userdata provides a __toString() function!

    -- Ask the server to provide the DataType node id for the variable (read "DataType" attribute)
    local dtNodeId = varNode.dataType
    local dtNode = client:getNode(dtNodeId)         -- Siemens: get the "DT_"<node> (which has the actual structure definition)
    print('dataType', tostring(dtNode))             -- print the DT_<node> info

    -- read the ExtensionObjectValue (as Variant)
    --[[
    -- NOTE: is not correctly working due to incomplete Variant implementation
    local ext = node:getExtensionObjectValue()
    ]]

    -- NOTE: reading varNode.Value or varNode.DataValue does *NOT* work correctly for structured types!
    --       so the following is nice, but does not correctly give any relevant values:
    local dv = varNode.dataValue
    print('dataValue', dv)
    local v = dv.value
    print('    Encoding', dv.binaryEncodingId)
    print('    Value', v:asValue())
    local bin = v:asBytes()
    print('    Binary', #bin,  basexx.to_hex(bin))

    -- Do it the correct way use the extension object data:
    -- Read the "ExtensionObject" raw data value (as binary encoded bytestring)
    local rawData, nidEncoding = varNode:getExtensionObject()                   -- Siemens: get the raw data and the "TE_"<node>
    print('ExtensionObject raw data:', basexx.to_hex(rawData))                  -- dump the (binary) encoded data
    local nodeEncoding = client:getNode(nidEncoding)                            -- ==> expect the "TE_"<node>
    print('                Encoding:', tostring(nodeEncoding))

    -- Read the "ExtensionObject" data type definition and add it to the data type cache
    local typeName, nidDataType = varNode:resolveExtensionObjectType()          -- prepare the node to decode the extension object
    local nodeDataType = client:getNode(nidDataType)                            -- ==> expect the "DT_"<node>
    print('                DataType:', tostring(nodeDataType))

    -- Dump the data type info (from the cache)
    local x = client:dumpType(typeName)

    -- Test encoding/decoding...
    local tbl1 = client:decodeExtensionObject(rawData, typeName)
    local newData = client:encodeExtensionObject(tbl1, typeName)
    if newData == rawData then
        print("Re-encoded data matches original data.")
    else
        print("Re-encoded data does NOT match original data.")
        print('Orig:', basexx.to_hex(rawData))
        print('Redo:', basexx.to_hex(newData))
    end

    -- try writing - this should now work!
    local ret = varNode:setExtensionObject(rawData, nidEncoding)
    if ret ~= 0 then
        print("Error writing extension object:", ret, opcua.getStatusCodeName(ret))
    else
        print("Extension object written successfully.")
    end

    -- local rawStruct = nodeDataType:getStructureDefinition()
    client:disconnect()
end

-- run the test
main_test_extension_object('opc.tcp://192.168.0.1:4840', 3, '"==300_LcsCom_he"."eventLogInOrOffUser"."request"')
