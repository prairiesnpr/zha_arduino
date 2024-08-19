#include <stdint.h>
#include "zha_constants.h"
#include <XBee.h>

bool *cmd_result;
bool cur_step_cmp = 1;

uint64_t SWAP_UINT64(uint64_t num)
{
  uint64_t byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7;
  byte0 = (num & 0x00000000000000FF) >> 0;
  byte1 = (num & 0x000000000000FF00) >> 8;
  byte2 = (num & 0x0000000000FF0000) >> 16;
  byte3 = (num & 0x00000000FF000000) >> 24;
  byte4 = (num & 0x000000FF00000000) >> 32;
  byte5 = (num & 0x0000FF0000000000) >> 40;
  byte6 = (num & 0x00FF000000000000) >> 48;
  byte7 = (num & 0xFF00000000000000) >> 56;
  return ((byte0 << 56) | (byte1 << 48) | (byte2 << 40) | (byte3 << 32) | (byte4 << 24) | (byte5 << 16) | (byte6 << 8) | (byte7 << 0));
}

class XbeeZha
{
public:
  XBeeWithCallbacks xbee;
  LocalMac macAddr = LocalMac(0);
  uint8_t dev_status = START;
  bool enabled = 0;

  uint8_t last_seq_id;
  Endpoint *endpoints;
  uint8_t num_endpoints;

  uint8_t cmd_seq_id;
  uint8_t cmd_frame_id;
  uint16_t netAddr;
  uint8_t seqID;
  void (*zha_clstr_cb)(ZBExplicitRxResponse &erx);
  void (*zha_write_attr_cb)(ZBExplicitRxResponse &erx);

  void Start(
      Stream &serial,
      void (*zha_clstr_callback)(ZBExplicitRxResponse &erx),
      void (*zha_write_attr_callback)(ZBExplicitRxResponse &erx),
      uint8_t NumEndpoints,
      Endpoint *EndpointsAddr

  )
  {

    num_endpoints = NumEndpoints;
    endpoints = EndpointsAddr;
    this->zha_clstr_cb = zha_clstr_callback;
    this->zha_write_attr_cb = zha_write_attr_callback;

    xbee.setSerial(serial);

    getMAC();
    Serial.print(F("LCL Add: "));
    printAddr(macAddr.Get());

    enabled = 1;
  }
  void registerCallbacks(
      void (*at_callback)(AtCommandResponse &erx, uintptr_t),
      void (*cmd_callback)(ZBTxStatusResponse &erx, uintptr_t),
      void (*other_callback)(XBeeResponse &erx, uintptr_t),
      void (*zdo_callback)(ZBExplicitRxResponse &erx, uintptr_t))
  {
    xbee.onZBTxStatusResponse(cmd_callback);
    xbee.onAtCommandResponse(at_callback);
    xbee.onOtherResponse(other_callback);
    xbee.onZBExplicitRxResponse(zdo_callback);
  }
  void getMAC()
  {
    uint8_t msb[4];
    uint8_t lsb[4];
    // AtCommandResponse atResponse = AtCommandResponse();
    AtCommandRequest atRequest = AtCommandRequest();
    bool success = 0;
    while (success == 0)
    {
      atRequest.setCommand((uint8_t *)shCmd);
      xbee.send(atRequest);
      success = waitforResponse(msb);
    }
    success = 0;
    while (success == 0)
    {
      atRequest.setCommand((uint8_t *)slCmd);
      xbee.send(atRequest);
      success = waitforResponse(lsb);
    }
    macAddr.Set(XBeeAddress64(packArray(msb), packArray(lsb)));
  }
  void loop()
  {
    xbee.loop();
    if (enabled)
    {
      if (cur_step_cmp)
      {
        cur_step_cmp = 0;
        if (dev_status == START)
        {
          Serial.println(F("Start Assc"));
          dev_status = ASSOCIATE;
          getAssociation();
        }
        else if (dev_status == ASSOCIATE)
        {
          Serial.println(F("NWK Pending"));
          dev_status = NWK;
          getNetAddr();
        }
        else if (dev_status == NWK)
        {
          dev_status = DEV_ANN;
          cmd_result = &cur_step_cmp;
          sendDevAnnounce();
        }
        else if (dev_status == DEV_ANN)
        {
          Serial.println("Ready");
          dev_status = READY;
        }
      }
    }
  }
  Endpoint GetEndpoint(uint8_t ep_id)
  {
    for (uint8_t i = 0; i < num_endpoints; i++)
    {
      if (endpoints[i].id == ep_id)
      {
        return endpoints[i];
      }
    }
    return endpoints[0]; // hmm, seems bad
  }
  void sendActiveEpResp(uint8_t cmd_seq_id)
  {
    /*
      byte 0 sequence number
      byte 1 status 00 success
      byte 2-3 NWK little endian
      byte 4 Number of active endpoints
      List of active endpoints
      ** Tested **
    */
     
    uint8_t buffer_len = 5 + num_endpoints;

    uint8_t buffer[buffer_len];
    memcpy(buffer, &cmd_seq_id, 1);
    memset(buffer + 1, CMD_SUCCESS, 1);
    memcpy(buffer + 2, &netAddr, 2);
    memcpy(buffer + 4, &num_endpoints, 1);

    uint8_t cl_i = 0;
    for (uint8_t i = 5; i < buffer_len; i++)
    {
      memset(buffer + i, endpoints[cl_i].id, 1);
      cl_i++;
    }

    Serial.println(F("Actv Ep Resp"));
    this->sendZHACmd(buffer, buffer_len, 0x00, 0x00, ACTIVE_EP_RSP, UKN_NET_ADDR, 0x0000);
    // If we are here. We are joined enough. move to ready
    this->dev_status = READY;
  }

  void sendDevAnnounce()
  {
    /*
      *** Device Announce ***
      12 byte data payload
      byte 0: Sequence Number
      byte 1-2: Net Addr in Little Endian
      byte 3-10: Mac Addr in Little Endian
      byte 11: Mac Capability Flag, 0x8C = Mains powered device; receiver on when idle; address not self-assigned.
      ** Tested **
    */
    uint64_t mac = macAddr.Get();
    uint8_t buffer_len = 12;
    uint8_t buffer[buffer_len];
    seqID++;
    memcpy(buffer, &seqID, 1);
    memcpy(buffer + 1, &netAddr, 2);
    memcpy(buffer + 3, &mac, 8);
    memcpy(buffer + 11, &MAINS_PWR_DEV, 1);
    Serial.println(F("Dev Ann"));
    this->sendZHACmd(buffer, buffer_len, 0x00, 0x00, 0x0013, 0xFFFD, 0x0000);
  }
  void sendAttributeRptMult(Cluster *cluster, uint16_t *attr_ids, uint8_t attridlen, uint8_t src_ep, uint8_t dst_ep)
  {

    /*
      payload
      byte 0: frame control
      byte 1 Seq
      byte 2 cmd id
      byte 3-4: Attr Id
      byte 5: type
      bytes[] value in little endian
      -----------------------------
      CMDS: 0x0A Report Attr
            0x01 Read Attr Response
            0x0D Discover Attributes Response
            0x04 Write Attr Response
      **   **
    */

    cmd_seq_id++;

    uint8_t buffer_len = 3; // 3 byte ZCL header

    uint8_t header_buffer[3];

    this->BuildZCLHeader(header_buffer, cmd_seq_id, REPORT_ATTRIBUTES);

    for (uint8_t i = 0; i < attridlen; i++)
    {
      uint8_t attr_rpt_len = 3; // 2 bytes for attrid + 1 byte for the data type
      attribute *attr;
      uint8_t attr_exists = cluster->GetAttr(&attr, attr_ids[i]);
      attr_rpt_len += attr->val_len;
      buffer_len += attr_rpt_len;
    }

    uint8_t buffer[buffer_len];
    memset(buffer, 0x00, buffer_len);
    memcpy(buffer, header_buffer, 3);
    uint8_t bufpos = 3;
    for (uint8_t i = 0; i < attridlen; i++)
    {
      attribute *attr;
      uint8_t attr_exists = cluster->GetAttr(&attr, attr_ids[i]);
      memset(buffer + bufpos, static_cast<uint8_t>((attr->id & 0x00FF) >> 0), 1);     // attr id lsb
      memset(buffer + bufpos + 1, static_cast<uint8_t>((attr->id & 0xFF00) >> 8), 1); // attr id msb
      memset(buffer + bufpos + 2, attr->type, 1);
      bufpos += 3;
      if (attr->type == ZCL_CHAR_STR)
      {
        memcpy(buffer + bufpos, &attr->val_len, 1); // Need to add the length of the string
        memcpy(buffer + bufpos + 1, attr->value, attr->val_len);
        bufpos += (1 + attr->val_len);
      }
      else
      {
        memcpy(buffer + bufpos, attr->value, attr->val_len);
        bufpos += (attr->val_len);
      }
    }
    

    Serial.println(F("Sent Mult Attr Rpt"));
    this->sendZHACmd(buffer, buffer_len, src_ep, dst_ep, cluster->id, COORDINATOR_NWK, HA_PROFILE_ID);
  }
  void sendAttributeRpt(uint16_t cluster_id, attribute *attr, uint8_t src_ep, uint8_t dst_ep)
  {

    /*
      payload
      byte 0: frame control
      byte 1 Seq
      byte 2 cmd id
      byte 3-4: Attr Id
      byte 5: type
      bytes[] value in little endian
      -----------------------------
      CMDS: 0x0A Report Attr
            0x01 Read Attr Response
            0x0D Discover Attributes Response
            0x04 Write Attr Response
      ** Tested  **
    */

    cmd_seq_id++;
    uint8_t buffer_len = ZCL_HDR_LEN + attr->val_len; // 6 is the length we will get back
    if (attr->type == ZCL_CHAR_STR)
    {
      buffer_len++; // Need to add a byte for the length of the string
    }
    uint8_t buffer[buffer_len];
    // Note, abusing this, since this isn't a cmd, we are setting the 6th byte to the attr type
    this->BuildPktStart(buffer, cluster_id, attr->id, REPORT_ATTRIBUTES, cmd_seq_id, attr->type, 0x00);

    if (attr->type == ZCL_CHAR_STR)
    {
      memcpy(buffer + ZCL_HDR_LEN, &attr->val_len, 1); // Need to add the length of the string
      memcpy(buffer + ZCL_HDR_LEN + 1, attr->value, attr->val_len);
    }
    else
    {
      memcpy(buffer + ZCL_HDR_LEN, attr->value, attr->val_len);
    }
    

    if (attr->type != 0)
    {
      Serial.println(F("Sent Attribute Rpt"));
      this->sendZHACmd(buffer, buffer_len, src_ep, dst_ep, cluster_id, COORDINATOR_NWK, HA_PROFILE_ID);
    }
  }
  void getAssociation()
  {
    AtCommandRequest atRequest = AtCommandRequest();
    atRequest.setCommand((uint8_t *)assocCmd);
    xbee.send(atRequest);
  }
  void sendAttributeCmdRsp(uint16_t cluster_id, attribute *attr, uint8_t src_ep, uint8_t dst_ep, uint8_t result, uint8_t rqst_seq_id)
  {
    /*
      Byte 0-2: ZCL Header
      Byte 3: Result
      Byte 4: CMD_SUCCESS
       ** Tested **

    */

    uint8_t buffer_len = 5;
    uint8_t buffer[buffer_len];
    this->BuildZCLHeader(buffer, rqst_seq_id, ATTR_RSP_CMD, 0x00);
    memcpy(buffer + 3, &result, 1);
    memset(buffer + 4, CMD_SUCCESS, 1);

    

    if (attr->type != 0)
    {
      Serial.print(F("Sent Attr Write Rsp: "));
      Serial.print(F(" Src EP: "));
      Serial.print(src_ep);
      Serial.print(F(" Dst EP: "));
      Serial.print(dst_ep);
      Serial.print(F(" Clstr ID: "));
      Serial.println(cluster_id);
      this->sendZHACmd(buffer, buffer_len, src_ep, dst_ep, cluster_id, COORDINATOR_NWK, HA_PROFILE_ID);
    }
  }
  void sendAttributeWriteResp(uint16_t cluster_id, attribute *attr, uint8_t src_ep, uint8_t dst_ep, uint8_t result, uint8_t rqst_seq_id)
  {
    /*
      Byte 0-2: ZCL Header
      Byte 3: write attr resp status record

       ** Tested **

    */

    uint8_t buffer_len = 4;
    uint8_t buffer[buffer_len];
    this->BuildZCLHeader(buffer, rqst_seq_id, WRITE_ATTR_RESP, 0x00);
    // memcpy(buffer + 3, &result, 1);
    memset(buffer + 3, CMD_SUCCESS, 1);

    

    if (attr->type != 0)
    {
      Serial.print(F("Sent Attr Write Rsp: "));
      Serial.print(F(" Src EP: "));
      Serial.print(src_ep);
      Serial.print(F(" Dst EP: "));
      Serial.print(dst_ep);
      Serial.print(F(" Clstr ID: "));
      Serial.println(cluster_id);
      this->sendZHACmd(buffer, buffer_len, src_ep, dst_ep, cluster_id, COORDINATOR_NWK, HA_PROFILE_ID);
    }
  }

  void sendAttributeRespMult(Cluster *cluster, uint16_t *attr_ids, uint8_t attridlen, uint8_t src_ep, uint8_t dst_ep)
  {

    /*
      payload
      byte 0: frame control
      byte 1 Seq  *Looks like this should match request
      byte 2 cmd id
      byte 3-4: Attr Id
      byte 5: type
      bytes[] value in little endian
      -----------------------------
      CMDS: 0x0A Report Attr
            0x01 Read Attr Response
            0x0D Discover Attributes Response
            0x04 Write Attr Response
      ** Tested **
      Not working for single missing attributes
    */
    uint8_t buffer_len = 3; // 3 byte ZCL header

    uint8_t header_buffer[3];

    this->BuildZCLHeader(header_buffer, cmd_seq_id, READ_ATTRIBUTES_RESP);

    for (uint8_t i = 0; i < attridlen; i++)
    {
      uint8_t attr_rpt_len = 3; // 2 bytes for attrid + 1 byte for status
      attribute *attr;
      uint8_t attr_exists = cluster->GetAttr(&attr, attr_ids[i]);
      if (attr_exists)
      {
       if (attr->type == ZCL_CHAR_STR)
        {
          attr_rpt_len += 1;             // 1 byte for string length
        }
        attr_rpt_len += 1;             // 1 byte for data type
        attr_rpt_len += attr->val_len; // x bytes for value
      }
      buffer_len += attr_rpt_len;
    }

    uint8_t buffer[buffer_len];
    memset(buffer, 0x00, buffer_len);
    memcpy(buffer, header_buffer, 3);
    uint8_t bufpos = 3;
    for (uint8_t i = 0; i < attridlen; i++)
    {
      attribute *attr;
      uint8_t attr_exists = cluster->GetAttr(&attr, attr_ids[i]);
      memset(buffer + bufpos, static_cast<uint8_t>((attr_ids[i] & 0x00FF) >> 0), 1); // attr id lsb
      bufpos++;
      memset(buffer + bufpos, static_cast<uint8_t>((attr_ids[i] & 0xFF00) >> 8), 1); // attr id msb
      bufpos++;

      if (attr_exists)
      {
        memset(buffer + bufpos, CMD_SUCCESS, 1); // status
        bufpos++;

        memset(buffer + bufpos, attr->type, 1); // data type
        bufpos++;

        if (attr->type == ZCL_CHAR_STR)
        {
          memcpy(buffer + bufpos, &attr->val_len, 1); // Need to add the length of the string
          bufpos++;
          memcpy(buffer + bufpos, attr->value, attr->val_len);
          bufpos += attr->val_len;
        }
        else
        {
          memcpy(buffer + bufpos, attr->value, attr->val_len);
          bufpos += attr->val_len;
        }
      }
      else
      {
        memset(buffer + bufpos, UNSUPPORTED_ATTRIBUTE, 1);
        bufpos++;
      }
    }
    

    Serial.println(F("Sent Mult Attr Read Resp"));
    this->sendZHACmd(buffer, buffer_len, src_ep, dst_ep, cluster->id, COORDINATOR_NWK, HA_PROFILE_ID);
  }

  void sendAttributeCfgRptResp(
      uint16_t cluster_id,
      uint16_t *attr_ids,
      uint8_t attridlen,
      uint8_t src_ep,
      uint8_t dst_ep,
      uint8_t rqst_seq_id)
  {
    /*
      Byte 0-2: ZCL Header
      Byte 3: attr status record

    */

    Serial.print(F("Id cnt: "));
    Serial.println(attridlen);
    uint8_t buffer_len = 3 + (4 * attridlen);
    uint8_t buffer[buffer_len];
    this->BuildZCLHeader(buffer, rqst_seq_id, CFG_RPT_RESP, 0x00);

    for (uint8_t i = 0; i < attridlen; i++)
    {
      memset(buffer + 3 + (i * 4), UNSUPPORTED_ATTRIBUTE, 1); // Not ok
      memset(buffer + 3 + (i * 4) + 1, 0x00, 1);              // Attrs are reported
      memcpy(buffer + 3 + (i * 4) + 2, &attr_ids[i], 2);
    }

    

    Serial.println(F("Sent Attr Rpt Cfg Rsp: "));

    this->sendZHACmd(buffer, buffer_len, src_ep, dst_ep, cluster_id, COORDINATOR_NWK, HA_PROFILE_ID);
  }

  void sendAttributeCfgRptRespAllOk(uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, uint8_t rqst_seq_id)
  {
    /*
      Byte 0-2: ZCL Header
      Byte 3: attr status record

       ** Tested **

    */

    uint8_t buffer_len = 4;
    uint8_t buffer[buffer_len];
    this->BuildZCLHeader(buffer, rqst_seq_id, CFG_RPT_RESP, 0x00);
    // memcpy(buffer + 3, &result, 1);
    memset(buffer + 3, CMD_SUCCESS, 1);

    

    Serial.print(F("Sent Attr Rpt Cfg Rsp - OK: "));
    Serial.print(F(" Src EP: "));
    Serial.print(src_ep);
    Serial.print(F(" Dst EP: "));
    Serial.print(dst_ep);
    Serial.print(F(" Clstr ID: "));
    Serial.println(cluster_id);
    this->sendZHACmd(buffer, buffer_len, src_ep, dst_ep, cluster_id, COORDINATOR_NWK, HA_PROFILE_ID);
  }
  void sendSimpleDescRpt(uint8_t ep, uint8_t rqst_cmd_seq_id)
  {
    /*
      ** Simple Descriptor Response **
      byte 0: Sequence number, match requesting packet
      byte 1: status 00= Success
      byte 2-3: NWK in little endian
      byte 4: Length of Simple Descriptor Report
      byte 5: End point report, which endpoint is being reported
      byte 6-7: profile id in little endian 0x0104
      byte 7-8: Device Type in little endian, 0x0007 is combined interface see page 51 of Zigbee HA profile
      byte 9: version number (App Dev)
      byte 10: Input Cluster Count
      byte [] List of input clusters in little endian format
      byte n+1: Output Cluster Count
      byte [] List of output clusters in little endian format
      ** Tested  **
    */

    
    uint8_t buffer_len = 11;

    uint8_t in_len = 2 * endpoints[(ep - 1)].GetNumInClusters();
    buffer_len++;
    if (in_len)
    {
      buffer_len += (in_len);
    }
    uint8_t out_len = 2 * endpoints[(ep - 1)].GetNumOutClusters();
    buffer_len++;
    if (out_len)
    {
      buffer_len += (out_len);
    }
    uint8_t buffer[buffer_len];
    memcpy(buffer, &rqst_cmd_seq_id, 1);
    memset(buffer + 1, 0x00, 1);
    memcpy(buffer + 2, &netAddr, 2);
    memset(buffer + 4, (out_len + in_len + 2), 1); // Length of simple descriptor
    memcpy(buffer + 5, &ep, 1);
    memcpy(buffer + 6, &HA_PROFILE_ID, 2);
    memset(buffer + 8, static_cast<uint8_t>((endpoints[(ep - 1)].GetDevType() & 0x00FF) >> 0), 1); // Fix me
    memset(buffer + 9, static_cast<uint8_t>((endpoints[(ep - 1)].GetDevType() & 0xFF00) >> 8), 1);
    memset(buffer + 10, 0x01, 1); // App version

    memset(buffer + 11, endpoints[(ep - 1)].GetNumInClusters(), 1);
    if (in_len)
    {
      endpoints[ep - 1].FillInCluster(buffer, 12);
    }
    memset(buffer + 12 + in_len, endpoints[(ep - 1)].GetNumOutClusters(), 1);

    if (out_len)
    {
      endpoints[ep - 1].FillOutCluster(buffer, 13 + in_len);
    }

    Serial.print(F("Send Smpl Desc Rpt: "));
    Serial.println(ep, HEX);
    this->sendZHACmd(buffer, buffer_len, 0x00, 0x00, SIMPLE_DESC_RSP, UKN_NET_ADDR, 0x0000);
  }
  void print_payload(uint8_t *payload, uint8_t len)
  {
    Serial.print(F("PLD: "));
    for (uint8_t i = 0; i < len; i++)
    {
      Serial.print(F("0x"));
      Serial.print(*(payload + i), HEX);
      Serial.print(F(" "));
    }
    Serial.println();
  }

private:
  void
  getNetAddr()
  {
    AtCommandRequest atRequest = AtCommandRequest();
    atRequest.setCommand((uint8_t *)netCmd);
    xbee.send(atRequest);
  }
  void printAddr(uint64_t val)
  {
    uint32_t msb = val >> 32;
    uint32_t lsb = val;
    Serial.print(msb, HEX);
    Serial.println(lsb, HEX);
  }

  bool waitforResponse(uint8_t *val)
  {
    AtCommandResponse atResponse = AtCommandResponse();
    if (xbee.readPacket(5000))
    {
      if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE)
      {
        xbee.getResponse().getAtCommandResponse(atResponse);
        if (atResponse.isOk())
        {
          if (atResponse.getValueLength() > 0)
          {
            for (int i = 0; i < atResponse.getValueLength(); i++)
            {
              val[i] = atResponse.getValue()[i];
            }
            return 1;
          }
        }
        else
        {
          Serial.print(F("Cmd rtrn err: "));
          Serial.println(atResponse.getStatus(), HEX);
        }
      }
      else
      {
        Serial.print(F("Exp AT got "));
        Serial.println(xbee.getResponse().getApiId(), HEX);
      }
    }
    else
    {
      // at command failed
      if (xbee.getResponse().isError())
      {
        Serial.print(F("Er rd pkt. EC: "));
        Serial.println(xbee.getResponse().getErrorCode());
      }
      else
      {
        Serial.println(F("No rsp"));
      }
    }
    return 0;
  }

  uint32_t packArray(uint8_t *val)
  {
    uint32_t res = 0;
    for (int i = 0; i < 4; i++)
    {
      res = res << 8 | val[i];
    }
    return res;
  }
  void BuildZCLHeader(uint8_t *buffer, uint8_t tran_seq_num, uint8_t cmd_id, bool global = 0x01)
  {
    /* ZCL Header (3 Bytes)
     byte 0: Frame control
       bits 0-1: Frame Type (Global 0b00, Cluster 0b01)
       bit 2: Manu Specific 0 for not included
       bit 3: Direction 0 for client (all currently implemented attributes are server to client, so hardcoded here)
       bit 4: Disable Def Response, 1 no resp if no error
       bit 5-7 Reserved
       So, we will always send
          Global:  0b00001000
          Cluster: 0b01001000
     byte 1-2 Manuf Code - Not included if excluded in frame control, we never send
     byte 3: Transaction Seq Number
     byte 4: Command Id
  */
    if (global)
    {
      memset(buffer, FRAME_CTRL_GLBL, 1);
    }
    else
    {
      memset(buffer, FRAME_CTRL_CLSTR, 1);
    }
    memcpy(buffer + 1, &tran_seq_num, 1);
    memcpy(buffer + 2, &cmd_id, 1);
  }

  void BuildPktStart(uint8_t *buffer, uint16_t cluster_id, uint16_t attr_id, uint8_t cmd_id, uint8_t tran_seq_num, uint8_t cmd_rslt, bool global)
  {
    /*
      ** Rename Function **
      byte 0-3: ZCL Header
      byte 4-5 Attr Id
      Byte 6 Command Result
    */

    // Will always fill in 6 bytes
    this->BuildZCLHeader(buffer, tran_seq_num, cmd_id, global);
    memset(buffer + 3, static_cast<uint8_t>((attr_id & 0x00FF) >> 0), 1); // attr id lsb
    memset(buffer + 4, static_cast<uint8_t>((attr_id & 0xFF00) >> 8), 1); // attr id msb
    memcpy(buffer + 5, &cmd_rslt, 1);
  }
  void sendZHACmd(uint8_t *buffer,
                  uint8_t buffer_len,
                  uint8_t src_ep,
                  uint8_t dst_ep,
                  uint16_t cluster_id,
                  uint16_t destination = COORDINATOR_NWK,
                  uint16_t profile_id = HA_PROFILE_ID)
  {
    cmd_frame_id = xbee.getNextFrameId();

    ZBExplicitTxRequest exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                                                     COORDINATOR_NWK,
                                                     0x00,         // broadcast radius
                                                     0x00,         // option
                                                     buffer,       // payload
                                                     buffer_len,   // payload length
                                                     cmd_frame_id, // frame ID
                                                     src_ep,       // src endpoint
                                                     dst_ep,       // dest endpoint
                                                     cluster_id,   // cluster ID
                                                     profile_id    // profile ID

    );

    xbee.send(exp_tx);
  }
};
