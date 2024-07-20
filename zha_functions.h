#include <stdint.h>
//#include "device_details.h"
#include <XBee.h>

uint8_t cmd_seq_id;
uint8_t t_payload[25] = {};
uint8_t cmd_frame_id;
uint8_t netAddr[2];
uint8_t seqID;
typedef void (*cmd_ptr)();
cmd_ptr last_command;
XBeeWithCallbacks xbee;
ZBExplicitTxRequest exp_tx = ZBExplicitTxRequest();

LocalMac macAddr = LocalMac(0);

uint16_t COORDINATOR_NWK = 0x0000;

#define SWAP_UINT16(x) (((x) >> 8) | ((x) << 8))
#define SWAP_UINT32(x) (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))

uint64_t SWAP_UINT64(uint64_t num)
{
  uint64_t byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7;
  byte0 = (num & 0x00000000000000FF) >> 0 ;
  byte1 = (num & 0x000000000000FF00) >> 8 ;
  byte2 = (num & 0x0000000000FF0000) >> 16 ;
  byte3 = (num & 0x00000000FF000000) >> 24 ;
  byte4 = (num & 0x000000FF00000000) >> 32 ;
  byte5 = (num & 0x0000FF0000000000) >> 40 ;
  byte6 = (num & 0x00FF000000000000) >> 48 ;
  byte7 = (num & 0xFF00000000000000) >> 56 ;
  return ((byte0 << 56) | (byte1 << 48) | (byte2 << 40) | (byte3 << 32) | (byte4 << 24) | (byte5 << 16) | (byte6 << 8) | (byte7 << 0));
}

Endpoint GetEndpoint(uint8_t ep_id)
{
  for (uint8_t i = 0; i < NUM_ENDPOINTS; i++) {
    if (ENDPOINTS[i].id == ep_id) {
      return ENDPOINTS[i];
    }
  }
}

void printAddr(uint64_t val) {
  uint32_t msb = val >> 32;
  uint32_t lsb = val;
  Serial.print(msb, HEX);
  Serial.println(lsb, HEX);
}

void printPayload(uint8_t *payload, uint8_t len) {
  for (int i = 0; i < len; i++) {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

bool waitforResponse(uint8_t *val) {
  AtCommandResponse atResponse = AtCommandResponse();
  if (xbee.readPacket(5000)) {
    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE) {
      xbee.getResponse().getAtCommandResponse(atResponse);
      if (atResponse.isOk()) {
        if (atResponse.getValueLength() > 0) {
          //Serial.print("Size of: ");
          //Serial.println(atResponse.getValueLength());
          for (int i = 0; i < atResponse.getValueLength(); i++) {
            val[i] = atResponse.getValue()[i];
          }
          return 1;
        }
      }
      else {
        Serial.print(F("Cmd rtrn err: "));
        Serial.println(atResponse.getStatus(), HEX);
      }
    } else {
      Serial.print(F("Exp AT got "));
      Serial.print(xbee.getResponse().getApiId(), HEX);
    }
  } else {
    // at command failed
    if (xbee.getResponse().isError()) {
      Serial.print(F("Er rd pkt. EC: "));
      Serial.println(xbee.getResponse().getErrorCode());
    }
    else {
      Serial.print(F("No rsp"));
    }
  }
  return 0;
}

void sendAttributeWriteRsp(uint16_t cluster_id, attribute* attr, uint8_t src_ep, uint8_t dst_ep,  uint8_t result) {
  /*
    payload
    byte 0: frame control
    byte 1 Seq
    byte 2 cmd id
    byte 3-4: Attr Id
    byte 5: type
    bytes6: Success 0x01
    -----------------------------
    CMDS: 0x0A Report Attr
          0x01 Read Attr Response
          0x0D Discover Attributes Response
          0x04 Write Attr Response

  */
  uint8_t payload_len = 4 + attr->val_len;

  uint8_t pre[payload_len] = {0x00,
                              cmd_seq_id,
                              0x0b,
                              result,
                              0x00
                             };


  memcpy(t_payload, pre, sizeof(pre));
  cmd_frame_id = xbee.getNextFrameId();

  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               COORDINATOR_NWK,
                               0x00,    //broadcast radius
                               0x00,    //option
                               t_payload, //payload
                               sizeof(pre),
                               //payload_len,    //payload length
                               cmd_frame_id, // frame ID
                               src_ep,    //src endpoint
                               dst_ep,    //dest endpoint
                               cluster_id,    //cluster ID
                               HA_PROFILE_ID //profile ID
                              );


  if (attr->type != 0) {
    xbee.send(exp_tx);
    Serial.print(F("Sent Attr Write Rsp: "));
    Serial.print(F(" Src EP: "));
    Serial.print(src_ep);
    Serial.print(F(" Dst EP: "));
    Serial.print(dst_ep);
    Serial.print(F(" Clstr ID: "));
    Serial.println(cluster_id);

    printPayload(t_payload, sizeof(pre));
  }
}

void sendAttributeRpt(uint16_t cluster_id, attribute* attr, uint8_t src_ep, uint8_t dst_ep) {
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

  */


  uint8_t payload_len;
  if (attr->type == ZCL_CHAR_STR){
  //  payload_len = 7 + attr->val_len;
    uint8_t pre[] = {0x00, 
                     cmd_seq_id,
                     0x0A, //Read attr resp
                     static_cast<uint8_t>((attr->id & 0x00FF) >> 0),
                     static_cast<uint8_t>((attr->id & 0xFF00) >> 8),
                     attr->type,
                     attr->val_len,
                     *attr->value
                    };
    memcpy(t_payload, pre, sizeof(pre));
    payload_len = sizeof(pre);
    //memcpy(t_payload + 7, attr->value, attr->val_len);
  }
  else {
    uint8_t pre[] = {0x00,
                     cmd_seq_id,
                     0x0A, //Read attr resp
                     static_cast<uint8_t>((attr->id & 0x00FF) >> 0),
                     static_cast<uint8_t>((attr->id & 0xFF00) >> 8),
                     attr->type,
                    // *attr->value
                    };
    memcpy(t_payload, pre, sizeof(pre));
    memcpy(t_payload + sizeof(pre), attr->value, attr->val_len);
    payload_len = sizeof(pre) + attr->val_len;
  }
  
  cmd_frame_id = xbee.getNextFrameId();

  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               COORDINATOR_NWK,
                               0x00,    //broadcast radius
                               0x00,    //option
                               t_payload, //payload
                               payload_len,    //payload length
                               cmd_frame_id, // frame ID
                               src_ep,    //src endpoint
                               dst_ep,    //dest endpoint
                               cluster_id,    //cluster ID
                               HA_PROFILE_ID //profile ID
                              );


  if (attr->type != 0) {
    xbee.send(exp_tx);
    printPayload(t_payload, payload_len);
    Serial.println(F("Sent Attribute Rpt"));
  }
}


void sendAttributeRsp(uint16_t cluster_id, attribute* attr, uint8_t src_ep, uint8_t dst_ep, uint8_t cmd) {
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

  */

  uint8_t payload_len;
  if (attr->type == ZCL_CHAR_STR){
    payload_len = 8 + attr->val_len;
    uint8_t pre[] = {0x00, 
                     cmd_seq_id,
                     cmd, //Read attr resp
                     static_cast<uint8_t>((attr->id & 0x00FF) >> 0),
                     static_cast<uint8_t>((attr->id & 0xFF00) >> 8),
                     0x00,//status
                     attr->type,
                     attr->val_len,
                    };
    memcpy(t_payload, pre, sizeof(pre));
    memcpy(t_payload + 8, attr->value, attr->val_len);
  }
  else {
    payload_len = 7 + attr->val_len;
    uint8_t pre[] = {0x00,
                     cmd_seq_id,
                     cmd, //Read attr resp
                     static_cast<uint8_t>((attr->id & 0x00FF) >> 0),
                     static_cast<uint8_t>((attr->id & 0xFF00) >> 8),
                     0x00,
                     attr->type,
                    };
    memcpy(t_payload, pre, sizeof(pre));
    memcpy(t_payload + 7, attr->value, attr->val_len);
  }
  
  // }



  cmd_frame_id = xbee.getNextFrameId();
  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               COORDINATOR_NWK,
                               0x00,    //broadcast radius
                               0x00,    //option
                               t_payload, //payload
                               //sizeof(t_payload),
                               payload_len,    //payload length
                               cmd_frame_id, // frame ID
                               src_ep,    //src endpoint
                               dst_ep,    //dest endpoint
                               cluster_id,    //cluster ID
                               HA_PROFILE_ID //profile ID
                              );


  if (attr->type != 0) {
    xbee.send(exp_tx);
    Serial.println(F("Sent Attr Rsp"));

  }
}

void sendDevAnnounce() {
  /*
     12 byte data payload
     byte 0: Sequence Number
     byte 1-2: Net Addr in Little Endian
     byte 3-10: Mac Addr in Little Endian
     byte 11: Mac Capability Flag, 0x8C = Mains powered device; receiver on when idle; address not self-assigned.
  */
  uint64_t mac = SWAP_UINT64(macAddr.Get());
  uint8_t payload[] = {seqID++,
                       netAddr[1],
                       netAddr[0],
                       static_cast<uint8_t>((mac & 0xFF00000000000000) >> 56),
                       static_cast<uint8_t>((mac & 0x00FF000000000000) >> 48),
                       static_cast<uint8_t>((mac & 0x0000FF0000000000) >> 40),
                       static_cast<uint8_t>((mac & 0x000000FF00000000) >> 32),
                       static_cast<uint8_t>((mac & 0x00000000FF000000) >> 24),
                       static_cast<uint8_t>((mac & 0x0000000000FF0000) >> 16),
                       static_cast<uint8_t>((mac & 0x000000000000FF00) >> 8),
                       static_cast<uint8_t>((mac & 0x00000000000000FF) >> 0),
                       0x8C
                      };

  cmd_frame_id = xbee.getNextFrameId();
  last_command = &sendDevAnnounce;

  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               0x00FD,
                               0x00,    //broadcast radius
                               0x00,    //option
                               payload, //payload
                               sizeof(payload),    //payload length
                               cmd_frame_id, // frame ID
                               0x00,    //src endpoint
                               0x00,    //dest endpoint
                               0x0013,    //cluster ID
                               0x0000 //profile ID
                              );

  xbee.send(exp_tx);
  Serial.println(F("Send Dev Ann"));
}

void build_payload_list(const uint16_t *values, const uint8_t v_size, uint8_t* res) {
  //Build byte Payload in little endian order
  *res = v_size / 2;
  uint8_t c = 0;
  for (uint8_t i = 1; i < (2 * v_size + 1); i += 2) {
    *(res + i) = static_cast<uint8_t>((values[c] & 0x00FF) >> 0);
    *(res + i + 1) = static_cast<uint8_t>((values[c] & 0xFF00) >> 8);
    c++;
  }
}


void print_payload(uint8_t* payload, uint8_t len) {
  Serial.print(F("PLD: "));
  for (uint8_t i = 0; i < len; i++) {
    if (i == 0) {
      Serial.print(*payload, HEX);
    }
    else {
      Serial.print(*(payload + i), HEX);
    }
  }
  Serial.println();
}

void sendActiveEpResp() {
  /*
     byte 0 sequence number
     byte 1 status 00 success
     byte 2-3 NWK little endian
     byte 4 Number of active endpoints
     List of active endpoints
  */
  cmd_frame_id = xbee.getNextFrameId();
  last_command = &sendActiveEpResp;
  uint8_t len_payload = 5 + NUM_ENDPOINTS;
  uint8_t payload[len_payload] = {cmd_seq_id, //Has to match requesting packet
                                  0x00,
                                  netAddr[1],
                                  netAddr[0],
                                  NUM_ENDPOINTS,
                                 };

  uint8_t i = 5;
  uint8_t cl_i = 0;
  for (i; i < len_payload; i++) {
    payload[i] = ENDPOINTS[cl_i].id;
    cl_i++;
  }

  Serial.print(F("Actv Ep Rsp Pld: "));
  for (int j = 0; j < len_payload; j++) {
    Serial.print(payload[j], HEX);
    Serial.print(" ");
  }
  Serial.println("");


  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               UKN_NET_ADDR,
                               0x00,    //broadcast radius
                               0x00,    //option
                               payload, //payload
                               len_payload,    //payload length
                               cmd_frame_id, // frame ID
                               0x00,    //src endpoint
                               0x00,    //dest endpoint
                               ACTIVE_EP_RSP,    //cluster ID
                               0x0000 //profile ID
                              );

  xbee.send(exp_tx);

  Serial.println(F("Send Actv Ep Resp"));



}

void sendSimpleDescRpt(uint8_t ep) {
  /*
     byte 0: Sequence number, match requesting packet
     byte 1: status 00= Success
     byte 2-3: NWK in little endian
     byte 4: Length of Simple Descriptor Report
     byte 5: End point report, which endpoint is being reported
     byte 6-7: profile id in little endian 0x0104
     byte 7-8: Device Type in little endian, 0x0007 is combined interface see page 51 of Zigbee HA profile
     byte 9: version number (App Dev)
     byte 10: Input Cluster Count
     byte [] List of output clusters in little endian format
     byte n+1: Output Cluster Count
     byte [] List of output clusters in little endian format
  */



  cmd_frame_id = xbee.getNextFrameId();
  uint8_t num_out = ENDPOINTS[(ep - 1)].GetNumOutClusters();  
  uint8_t out_len = 0;
  if (num_out > 0)
  {
     out_len = 2 *num_out + 1;
  }
  uint8_t num_in = ENDPOINTS[(ep - 1)].GetNumInClusters();  
  uint8_t in_len = 0;
  if (num_in > 0)
  {
     in_len = 2 * num_in + 1;
  }
  uint8_t pre_len = 11;
  uint8_t payload_len = pre_len + out_len + in_len;

  uint8_t pre[] = {cmd_seq_id,
                   0x00,
                   netAddr[1],
                   netAddr[0],
                   static_cast<uint8_t>(out_len + in_len + 6), //Length of simple descriptor
                   ep,
                   static_cast<uint8_t>((HA_PROFILE_ID & 0x00FF) >> 0),
                   static_cast<uint8_t>((HA_PROFILE_ID & 0xFF00) >> 8),
                   static_cast<uint8_t>((ENDPOINTS[(ep - 1)].GetDevType() & 0x00FF) >> 0), //Fix me
                   static_cast<uint8_t>((ENDPOINTS[(ep - 1)].GetDevType() & 0xFF00) >> 8),
                   0x01, //Don't Care (App Version)
                  };

  uint8_t in_clusters[in_len];

  memcpy(t_payload, pre, pre_len);

  uint16_t in_cl[num_in];
  ENDPOINTS[(ep - 1)].GetInClusters(in_cl);
  build_payload_list(in_cl, 2 * num_in, in_clusters);
 
  memcpy(t_payload + pre_len , in_clusters, sizeof(in_clusters));

  uint8_t out_clusters[out_len];
  uint16_t out_cl[num_out];
  ENDPOINTS[(ep - 1)].GetOutClusters(out_cl);
  build_payload_list(out_cl, 2 * num_out, out_clusters);
 
  memcpy(t_payload + pre_len + sizeof(in_clusters) , out_clusters, sizeof(out_clusters));

  exp_tx = ZBExplicitTxRequest(COORDINATOR64,
                               UKN_NET_ADDR,
                               0x00,    //broadcast radius
                               0x00,    //option
                               t_payload, //payload
                               sizeof(t_payload),
                               //payload_len,    //payload length
                               cmd_frame_id, // frame ID
                               0x00,    //src endpoint
                               0x00,    //dest endpoint
                               SIMPLE_DESC_RSP,    //cluster ID
                               0x0000 //profile ID
                              );

  xbee.send(exp_tx);

  Serial.print(F("Send Smpl Desc Rpt: "));
  Serial.println(ep, HEX);

  printPayload(t_payload, sizeof(t_payload));
}

void sendBasicClusterResp() {
}


void getNetAddr() {
  AtCommandRequest atRequest = AtCommandRequest();
  atRequest.setCommand((uint8_t*)netCmd); //breaking(uint8_t*)at
  xbee.send(atRequest);
}

uint32_t packArray(uint8_t *val) {
  uint32_t res = 0;
  for (int i = 0; i < 4; i++) {
    res = res << 8 | val[i];
  }
  return res;
}

void getAssociation() {
  AtCommandRequest atRequest = AtCommandRequest();
  atRequest.setCommand((uint8_t*)assocCmd); //breaking
  xbee.send(atRequest);
}

void getMAC() {
  uint8_t msb[4];
  uint8_t lsb[4];
  AtCommandResponse atResponse = AtCommandResponse();
  AtCommandRequest atRequest = AtCommandRequest();
  bool success = 0;
  while (success == 0) {
    atRequest.setCommand((uint8_t*)shCmd); //breaking
    xbee.send(atRequest);
    success = waitforResponse(msb);
  }
  success = 0;
  while (success == 0) {
    atRequest.setCommand((uint8_t*)slCmd); //breaking
    xbee.send(atRequest);
    success = waitforResponse(lsb);
  }
  macAddr.Set(XBeeAddress64(packArray(msb), packArray(lsb)));
}

