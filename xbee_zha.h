#include "zha_functions.h"

XbeeZha zha;

void zbTxStatusResp(ZBTxStatusResponse &resp, uintptr_t)
{
  if (resp.isSuccess())
  {
    Serial.println(F("TX OK"));
    *cmd_result = 1;
  }
  else
  {
    Serial.println(F("TX FAIL"));
    Serial.println(resp.getDeliveryStatus(), HEX);

    if (resp.getFrameId() == zha.cmd_frame_id)
    {
      if (zha.dev_status == DEV_ANN)
      {
        zha.sendDevAnnounce();
      }
      else
      {
        zha.sendActiveEpResp(zha.last_seq_id);
      }
    }
  }
}

void otherResp(XBeeResponse &resp, uintptr_t)
{
  Serial.println(F("Other Response: "));
}

void atCmdResp(AtCommandResponse &resp, uintptr_t)
{
  if (resp.getStatus() == AT_OK)
  {
    if (resp.getCommand()[0] == assocCmd[0] &&
        resp.getCommand()[1] == assocCmd[1])
    {
      // Association Status
      if (resp.getValue()[0] != 0x00)
      {
        zha.getAssociation();
      }
      else
      {
        cur_step_cmp = 1;
      }
    }
    else if (resp.getCommand()[0] == netCmd[0] &&
             resp.getCommand()[1] == netCmd[1])
    {
      if (zha.netAddr != 0xFFFE)
      {
        memcpy(&zha.netAddr, resp.getValue(), 2);
        zha.netAddr = SWAP_UINT16(zha.netAddr);
        Serial.print(F("NWK: 0x"));
        Serial.print(resp.getValue()[0], HEX);
        Serial.println(resp.getValue()[1], HEX);
        cur_step_cmp = 1;
      }
    }
    else
    {
      Serial.println(F("Ukn Cmd"));
    }
  }
  else
  {
    Serial.println(F("AT Fail"));
  }
}
void printDiagnostic(ZBExplicitRxResponse &erx)
{
  Serial.print(F("ZDO: EP: "));
  Serial.print(erx.getDstEndpoint());
  Serial.print(F(", Clstr: "));
  Serial.print(erx.getClusterId(), HEX);
  Serial.print(F(" Cmd Id: "));
  Serial.print(erx.getFrameData()[erx.getDataOffset() + 2], HEX);
  Serial.print(F(" FrmCtl: "));
  Serial.print(erx.getFrameData()[erx.getDataOffset()], BIN);
  Serial.print(F(" Dir: "));
  if ((erx.getFrameData()[erx.getDataOffset()] >> 3) & 1)
  {
    Serial.println(F("Clnt"));
  }
  else
  {
    Serial.println(F("Srv"));
  }
  zha.print_payload(erx.getFrameData(), erx.getFrameDataLength());

}
void zdoReceive(ZBExplicitRxResponse &erx, uintptr_t)
{
  // Create a reply packet containing the same data
  Serial.println(F("ZDO Cmd"));
  printDiagnostic(erx);

  if (erx.getRemoteAddress16() == 0) // Why did I care about this again?
  {
    zha.cmd_seq_id = erx.getFrameData()[erx.getDataOffset() + 1];
    Serial.print(F("Cmd Seq: "));
    Serial.println(zha.cmd_seq_id);

    uint8_t cmd_id = erx.getFrameData()[erx.getDataOffset() + 2];

    if (erx.getFrameData()[erx.getDataOffset()] & 0x03) // frame type
    {
      // Cluster command, pass to app
      zha.zha_clstr_cb(erx);
    }
    else
    {
      // Handle global commands here
      Serial.println(F("Glbl Cmd"));

      Endpoint end_point = zha.GetEndpoint(erx.getDstEndpoint());
      if (cmd_id == READ_ATTRIBUTES)
      {
        if (end_point.ClusterExists(erx.getClusterId()))
        {
          // Read attributes
          uint8_t len_data = erx.getDataLength() - 3;
          uint16_t cur_attr_id;
          for (uint8_t i = erx.getDataOffset() + 3; i < (len_data + erx.getDataOffset() + 3); i += 2)
          {
            cur_attr_id = (erx.getFrameData()[i + 1] << 8) |
                          (erx.getFrameData()[i] & 0xff);

            Serial.print(F("Rd Att: "));
            Serial.println(cur_attr_id, HEX);

            attribute *attr;
            uint8_t attr_exists = end_point.GetCluster(erx.getClusterId()).GetAttr(&attr, cur_attr_id);

            if(attr_exists)
            {
              // Exists
              zha.sendAttributeRsp(erx.getClusterId(), attr, erx.getDstEndpoint(), 0x01, 0x01, zha.cmd_seq_id);
            }
            else
            {
              // Not found
              zha.sendAttributeRspFail(erx.getClusterId(), cur_attr_id, erx.getDstEndpoint(), 0x01, 0x01, zha.cmd_seq_id, UNSUPPORTED_ATTRIBUTE);
            }
            zha.cmd_seq_id++;
          }
        }
        else
        {
          Serial.println("Clstr Err");
        }
      }
      else if (cmd_id == WRITE_ATTRIBUTES)
      {
        zha.zha_write_attr_cb(erx);
      }
      else if (cmd_id == DEF_RESP)
      {
        // Default Response
        Serial.println(F("Def Rsp"));
      }
      else if (cmd_id == READ_RPT_CFG)
      {
        Serial.println(F("Rd Rpt Cfg"));
        // READ_RPT_CFG_RESP
        uint8_t len_data = erx.getDataLength() - 3;
        uint16_t attr_rqst[len_data / 2];
        for (uint8_t i = erx.getDataOffset() + 3; i < (len_data + erx.getDataOffset() + 3); i += 2)
        {
          attr_rqst[i / 2] = (erx.getFrameData()[i + 1] << 8) |
                             (erx.getFrameData()[i] & 0xff);

          Serial.print(F("Clstr Rd Att: "));
          Serial.println(attr_rqst[i / 2], HEX);
        }
      }
      else if (cmd_id == CFG_RPT)
      {
        Serial.println(F("Cfg Rpt Cmd"));
      }
      else
      {
        // Unsupported Global Command
        Serial.print(F("Unsprt Cmd ID: "));
        Serial.println(cmd_id, HEX);
      }
    }
    
    if (erx.getClusterId() == ACTIVE_EP_RQST)
    {
      // Have to match sequence number in response
      cmd_result = NULL;
      zha.last_seq_id = erx.getFrameData()[erx.getDataOffset()];
      zha.sendActiveEpResp(zha.last_seq_id);
    }
    if (erx.getClusterId() == SIMPLE_DESC_RQST)
    {
      Serial.print("Simple Desc Rqst, Ep: ");
      // Have to match sequence number in response
      // Payload is EndPoint
      // Can this just be regular ep?
      uint8_t ep_msg = erx.getFrameData()[erx.getDataOffset() + 3];
      Serial.println(ep_msg, HEX);
      zha.sendSimpleDescRpt(ep_msg, erx.getFrameData()[erx.getDataOffset()]);
    }
  }
  else
  {
    // Not from coordinator?
    Serial.println(F("Rcv Non Cdr Msg"));
  }
}
