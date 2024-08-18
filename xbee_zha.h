#include "zha_functions.h"

XbeeZha zha;

void zbTxStatusResp(ZBTxStatusResponse &resp, uintptr_t)
{
  if (resp.isSuccess())
  {
    Serial.println(F("TX OK"));
    if (cmd_result != nullptr)
    {
      *cmd_result = 0x01;
    }
  }
  else
  {
    Serial.println(F("TX FAIL"));
    Serial.println(resp.getDeliveryStatus(), HEX);
    if (cmd_result != nullptr)
    {
      *cmd_result = 0x00;
    }
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
uint8_t getAttrCfgRecLen(uint8_t direction, uint8_t attr_data_type)
{
  uint8_t reclen = 3; // 1 byte for direction, 2 bytes for attrid
  if (direction == 0x01)
  {
    // Serial.println(F("ToSrv"));
    reclen += 2; // 2 bytes for timeout
  }
  else
  {
    reclen += 5; // 1 bytes data type, 2 bytes for min rpt int, 2 bytes for max rpt int
    // Serial.println(F("ToClnt"));
    if (
        (attr_data_type > 0x07 && attr_data_type < 0x20) ||
        (attr_data_type > 0x2f && attr_data_type < 0x38) ||
        (attr_data_type > 0xe2)

    )
    {
      // skip it's discrete
      // Serial.println(F("discrete"));
    }
    else if (attr_data_type > 0x1f && attr_data_type < 0x28)
    {
      reclen += (attr_data_type - 0x1f);
      // Serial.println(F("uint"));
    }
    else if (attr_data_type > 0x27 && attr_data_type < 0x30)
    {
      reclen += (attr_data_type - 0x27);
      // Serial.println(F("int"));
    }
    else if (attr_data_type == 0x38)
    {
      reclen += 2;
      // Serial.println(F("semi"));
    }
    else if (attr_data_type == 0x39)
    {
      reclen += 4;
      // Serial.println(F("single"));
    }
    else if (attr_data_type == 0x3a)
    {
      reclen += 8;
      // Serial.println(F("double"));
    }
    else
    {
      reclen += 2;
    }
  }
  return reclen;
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
  if (erx.getDstEndpoint() == 0)
  {
    if (erx.getClusterId() == ACTIVE_EP_RQST)
    {
      // Have to match sequence number in response
      cmd_result = nullptr;
      zha.last_seq_id = erx.getFrameData()[erx.getDataOffset()];
      zha.sendActiveEpResp(zha.last_seq_id);
    }
    else if (erx.getClusterId() == SIMPLE_DESC_RQST)
    {
      Serial.print("Simple Desc Rqst, Ep: ");
      // Have to match sequence number in response
      // Payload is EndPoint
      // Can this just be regular ep?
      uint8_t ep_msg = erx.getFrameData()[erx.getDataOffset() + 3];
      Serial.println(ep_msg, HEX);
      zha.sendSimpleDescRpt(ep_msg, erx.getFrameData()[erx.getDataOffset()]);
    }
    else if (erx.getClusterId() == NODE_DESC_RESP_CMD)
    {
      Serial.println(F("Node Desc Rsp"));
    }
    else if (erx.getClusterId() == IEEE_ADDR_RESP_CMD)
    {
      Serial.println(F("IEEE Rsp"));
    }
    else if (erx.getClusterId() == DEV_ANN_CMD)
    {
      Serial.println(F("R Derv An"));
    }

    else
    {
      zha.print_payload(erx.getFrameData() + erx.getDataOffset(), erx.getDataLength());
      Serial.print(F("ZDO: EP: "));
      Serial.print(erx.getDstEndpoint());
      Serial.print(F(", Clstr: "));
      Serial.print(erx.getClusterId(), HEX);
      Serial.println(F(" Cmd Id: "));

      Serial.println(F("Handle Me?"));
    }
  }

  else if (erx.getRemoteAddress16() == 0) // Why did I care about this again?
  {
    printDiagnostic(erx);

    zha.cmd_seq_id = erx.getFrameData()[erx.getDataOffset() + 1];
    Serial.print(F("Cmd Seq: "));
    Serial.println(zha.cmd_seq_id, HEX);

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
      // This needs redone to send as one response, similar to rpt.

      Endpoint end_point = zha.GetEndpoint(erx.getDstEndpoint());
      if (cmd_id == READ_ATTRIBUTES)
      {
        Serial.println(F("RD Attr Cmd"));
        if (end_point.ClusterExists(erx.getClusterId()))
        {
          Cluster cluster = end_point.GetCluster(erx.getClusterId());

          // Read attributes
          uint8_t attr_count = 0;
          uint16_t cur_attr_id;
          for (uint8_t i = 3; i < (erx.getDataLength()); i += 2)
          {
            cur_attr_id = (erx.getData()[i + 1] << 8) |
                          (erx.getData()[i] & 0xff);


            attr_count++;
          }
          uint16_t attr_ids[attr_count] = {};
          uint8_t attrpos = 0;
          for (uint8_t i = 3; i < (erx.getDataLength()); i += 2)
          {
            cur_attr_id = (erx.getData()[i + 1] << 8) |
                          (erx.getData()[i] & 0xff);


            attr_ids[attrpos] = cur_attr_id;
            attrpos++;
          }
          Serial.print(F("Attr: "));
          for(uint8_t j = 0; j<attr_count; j++){
            Serial.print(attr_ids[j], HEX);
            Serial.print(F(", "));
          }
          Serial.println();
          zha.sendAttributeRespMult(&cluster, attr_ids, attr_count, end_point.id, 1);
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
        // This works but not using, so commented out to save space
        uint16_t cur_attr_id;
        uint8_t attr_data_type;
        uint8_t direction;
        /*
        uint16_t min_rpt_int;
        uint16_t max_rpt_int;
        */

        uint8_t bad_att_count = 0;
        uint8_t pktpos = 3;
        while (pktpos < erx.getDataLength())
        {
          direction = erx.getData()[pktpos];
          cur_attr_id = (erx.getData()[pktpos + 2] << 8) |
                        (erx.getData()[pktpos + 1] & 0xff);
          attr_data_type = erx.getData()[pktpos + 3];
          uint8_t rec_length = getAttrCfgRecLen(direction, attr_data_type);
          pktpos += rec_length;
          if (!zha.GetEndpoint(erx.getDstEndpoint()).GetCluster(erx.getClusterId()).AttributeExists(cur_attr_id))
          {
            bad_att_count++;
          }
        }

        uint16_t bad_attr_ids[bad_att_count];
        bad_att_count = 0;
        pktpos = 3;
        Serial.print(F("Unsup Attr: "));

        while (pktpos < erx.getDataLength())
        {
          direction = erx.getData()[pktpos];
          cur_attr_id = (erx.getData()[pktpos + 2] << 8) |
                        (erx.getData()[pktpos + 1] & 0xff);
          attr_data_type = erx.getData()[pktpos + 3];
          uint8_t rec_length = getAttrCfgRecLen(direction, attr_data_type);
          pktpos += rec_length;
          if (!zha.GetEndpoint(erx.getDstEndpoint()).GetCluster(erx.getClusterId()).AttributeExists(cur_attr_id))
          {
            bad_attr_ids[bad_att_count] = cur_attr_id;
            bad_att_count++;
            Serial.print(cur_attr_id, HEX);
            Serial.print(F(", "));
          }
        }
        Serial.println();

        /*
                  min_rpt_int = (erx.getFrameData()[i + 4] << 8) |
                                (erx.getFrameData()[i + 3] & 0xff);
                  max_rpt_int = (erx.getFrameData()[i + 6] << 8) |
                                (erx.getFrameData()[i + 5] & 0xff);*/

        zha.sendAttributeCfgRptResp(erx.getClusterId(), bad_attr_ids, bad_att_count, erx.getDstEndpoint(), 0x01, zha.cmd_seq_id);
      }
      else
      {
        // Unsupported Global Command
        Serial.print(F("Unsprt Cmd ID: "));
        Serial.println(cmd_id, HEX);
      }
    }
  }
  else
  {
    // Not from coordinator?
    Serial.println(F("Rcv Non Cdr Msg"));
  }
}
