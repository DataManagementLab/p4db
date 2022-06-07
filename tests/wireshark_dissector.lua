local p4db_protocol = Proto("P4DB", "P4DB Protocol")

local msg_type = ProtoField.int32("p4db.msgtype", "MessageType", base.DEC)
local msg_size = ProtoField.uint32("p4db.size", "Size", base.DEC)
local clientGlobalRank = ProtoField.uint32("p4db.clientGlobalRank", "clientGlobalRank", base.DEC)
local transactionNumber = ProtoField.uint64("p4db.transactionNumber", "transactionNumber", base.DEC)

-- LockGrant fields
local lockId = ProtoField.uint64("p4db.lockId", "lockId", base.DEC)
local mode = ProtoField.int32("p4db.lockMode", "lockMode", base.DEC)
local granted = ProtoField.int32("p4db.granted", "granted", base.DEC)
local expiryTime = ProtoField.uint64("p4db.expiryTime", "expiryTime", base.DEC)


p4db_protocol.fields = {
    msg_type,
    msg_size,
    clientGlobalRank,
    transactionNumber,

    lockId,
    mode,
    granted,
}


function p4db_protocol.dissector(buffer, pinfo, tree)
  length = buffer:len()
  if length == 0 then return end

  pinfo.cols.protocol = p4db_protocol.name

  local subtree = tree:add(p4db_protocol, buffer(), "P4DB Protocol Data")

  local type = buffer(0,4):le_int()
  local type_name = get_msg_type(type)
  subtree:add_le(msg_type, buffer(0,4)):append_text(" (" .. type_name .. ")")

  subtree:add_le(msg_size, buffer(4,4))
  subtree:add_le(clientGlobalRank, buffer(8,4))
  subtree:add_le(transactionNumber, buffer(16,8))

  if type_name == 'LOCK_GRANT' then
    subtree:add_le(lockId, buffer(24,8))
    subtree:add_le(mode, buffer(32,4))
    subtree:add_le(granted, buffer(36,4))
  elseif type_name == 'LOCK_REQUEST' then
    subtree:add_le(lockId, buffer(24,8))
    subtree:add_le(mode, buffer(32,4))
  end
end


function get_msg_type(type)
    local name = "Unknown"

    if type == 0x0000 then name = "HELLO"
    elseif type == 0x0001 then name = "SHUTDOWN"
    elseif type == 0x1000 then name = "LOCK_REQUEST"
    elseif type == 0x1001 then name = "LOCK_RELEASE"
    elseif type == 0x1002 then name = "VOTE_REQUEST"
    elseif type == 0x1003 then name = "TX_END"
    elseif type == 0x1004 then name = "TX_ABORT"
    elseif type == 0x2000 then name = "LOCK_GRANT"
    elseif type == 0x2001 then name = "VOTE_RESPONSE"
    elseif type == 0x2002 then name = "TRANSACTION_END_RESPONSE" end
  
    return name
  end



local eth_type = DissectorTable.get("ethertype")
eth_type:add(0x1001, p4db_protocol)


local udp = DissectorTable.get("udp.port")
udp:add(4000, p4db_protocol)


  