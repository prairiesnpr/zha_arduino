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
      //zha.last_command();
      if (zha.dev_status == DEV_ANN){
        zha.sendDevAnnounce();
      }
      else {
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
  // Serial.println(F("At resp"));
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
      if (zha.netAddr[0] != 0xff && zha.netAddr[1] != 0xFE)
      {
        zha.netAddr[0] = resp.getValue()[0];
        zha.netAddr[1] = resp.getValue()[1];
        Serial.print(F("NWK: 0x"));
        Serial.print(zha.netAddr[0], HEX);
        Serial.println(zha.netAddr[1], HEX);
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

//zha.registerCallbacks(atCmdResp, zbTxStatusResp, otherResp);
