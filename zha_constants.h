#include <stdint.h>
#include <EEPROM.h>
#include <XBee.h>

// Used to keep SRAM down
#define ZCL_HDR_LEN 6


// Device Type
constexpr uint8_t MAINS_PWR_DEV = 0x8c;

// Frame Control
#define FRAME_CTRL_GLBL 0b00011000
#define FRAME_CTRL_CLSTR 0b01011000

#define UKN_NET_ADDR 0xFFFE
// #define HA_PROFILE_ID 0x0104
constexpr uint16_t HA_PROFILE_ID = 0x0104;

#define COORDINATOR_NWK 0x0000

#define MATCH_DESC_RQST 0x0006
#define MATCH_DESC_RSP 0x8006
#define SIMPLE_DESC_RSP 0x8004
#define ACTIVE_EP_RSP 0x8005
#define ACTIVE_EP_RQST 0x0005
#define SIMPLE_DESC_RQST 0x0004

#define READ_ATTRIBUTES 0x00
#define DEF_RESP 0x0B
#define WRITE_ATTRIBUTES 0x02
#define REPORT_ATTRIBUTES 0x0a
#define WRITE_ATTR_RESP 0x04
#define READ_RPT_CFG 0x08
#define READ_RPT_CFG_RESP 0x09
#define CFG_RPT 0x06
#define CFG_RPT_RESP 0x07

// Command resp
#define ATTR_RSP_CMD 0x0b

// Input Clusters
#define BASIC_CLUSTER_ID 0x0000 //server
#define IDENTIFY_CLUSTER_ID 0x0003
#define GROUPS_CLUSTER_ID 0x0004 //server
#define SCENES_CLUSTER_ID 0x0005 //server
#define ON_OFF_CLUSTER_ID 0x0006 //server
#define ON_OF_SWITCH_CLUSTER_ID 0x0007 //server
#define LEVEL_CONTROL_CLUSTER_ID 0x0008 //server
#define LIGHT_LINK_CLUSTER_ID 0x1000
#define TEMP_CLUSTER_ID 0x0402 //server
#define HUMIDITY_CLUSTER_ID 0x405 //server
#define BINARY_INPUT_CLUSTER_ID 0x000f //server
#define IAS_ZONE_CLUSTER_ID 0x0500 //server
#define METERING_CLUSTER_ID 0x0702 // Smart Energy Metering //server
#define COLOR_CLUSTER_ID 0x0300 //server
#define ELECTRICAL_MEASUREMENT 0x0b04 //server
#define ANALOG_IN_CLUSTER_ID 0x000c //server
#define ANALOG_OUT_CLUSTER_ID 0x000d //server
#define MULTISTATE_IN_CLUSTER_ID 0x0012 //server 


// Attr id
#define INSTANTANEOUS_DEMAND 0x0400
#define RMS_CURRENT 0x0508
#define AC_FREQUENCY 0x0300
#define AC_FREQUENCY_MAX 0x0302
#define RMS_VOLTAGE 0x0505
#define RMS_VOLTAGE_MAX 0x0507
#define MEASUREMENT_TYPE 0x0000
#define MANUFACTURER_ATTR 0x0004
#define MODEL_ATTR 0x0005
#define BINARY_PV_ATTR 0x0055 // Should rename
#define BINARY_STATUS_FLG 0x006F
#define CURRENT_STATE 0x0000
#define IAS_ZONE_STATE 0x0000
#define IAS_ZONE_TYPE 0x0001
#define IAS_ZONE_STATUS 0x0002
#define OUT_OF_SERVICE 0x0051
#define RESOLUTION_ATTR 0x006A
#define ENG_UNITS_ATTR 0x0075
#define DESCRIPTION_ATTR 0x001C
#define MAX_PV_ATTR 0x0041
#define MIN_PV_ATTR 0x0045
#define NUM_OF_STATES 0x004A
#define DESCRIPTION_ATTR 0x001C
#define STATE_TEXT_ATTR 0x000E
// Output
#define OTA_CLUSTER_ID 0x0019 // Upgrade

// Data Types
#define ZCL_INT16_T 0x29 // Signed Analog
#define ZCL_CHAR_STR 0x42
#define ZCL_UINT8_T 0x20
#define ZCL_UINT16_T 0x21
#define ZCL_BOOL 0x10
#define ZCL_ENUM8 0x30
#define ZCL_ENUM16 0x31
#define ZCL_MAP8 0x18
#define ZCL_MAP16 0x19
#define ZCL_MAP32 0x1b
#define ZCL_SINGLE 0x39
#define ZCL_ARRAY 0x48  //Length 2+sum of length of contents , first element is 2byte uint for num elements 

// Device
#define ON_OFF_LIGHT 0x0100
#define DIMMABLE_LIGHT 0x0101
#define COLOR_LIGHT 0x0102
#define TEMPERATURE_SENSOR 0x0302
#define ON_OFF_OUTPUT 0x0002
#define IAS_ZONE 0x0402
#define ON_OFF_SENSOR 0x0850
#define COMBINED_INTERFACE 0x0007
// Attributes
#define ATTR_CURRENT_X 0x0003
#define ATTR_CURRENT_Y 0x0004
#define ATTR_CURRENT_CT_MRDS 0x0006

// Define Steps
#define START 0
#define ASSOCIATE 1
#define NWK 2
#define CFG_CMP 3
#define DEV_ANN 4
#define READY 5

// Attribute Write Responses
#define CMD_SUCCESS 0x00
#define CMD_FAILURE 0x01
#define UNSUP_CLUSTER_COMMAND 0x81
#define UNSUP_GENERAL_COMMAND 0x82
#define UNSUPPORTED_ATTRIBUTE 0x86

// CMD types
#define CMD_CLUSTER 0x01
#define CMD_GLOBAL 0x00

#define SWAP_UINT16(x) (((x) >> 8) | ((x) << 8))
#define SWAP_UINT32(x) (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))


class attribute
{
public:
  uint16_t id;
  uint8_t *value;
  uint8_t val_len;
  uint8_t type;
  uint8_t is_const;
  attribute(uint16_t a_id, uint8_t *a_value, uint8_t a_val_len, uint8_t a_type, bool is_const = 0x00)
  {
    this->id = a_id;
    this->val_len = a_val_len;
    this->type = a_type;
    this->is_const = is_const;

    if (is_const)
    {
      this->value = new uint8_t;
      this->value = a_value;
    }
    else
    {
      this->value = new uint8_t[a_val_len];
      memcpy(this->value, a_value, a_val_len);
    }
  }

  void SetValue(uint32_t new_value)
  {
    for (uint8_t i = 0; i < val_len; i++)
    {
      value[i] = ((uint32_t)new_value >> (i * 8)) & 0xff;
    }
  }

  uint32_t GetIntValue(uint8_t pr = 0x01)
  {
    uint32_t res_int = 0;
    if (val_len == 1)
    {
      res_int = (uint32_t)value[0];
    }
    if (val_len == 2)
    {
      res_int = (uint32_t)value[0] | ((uint32_t)value[1] << 8);
    }
    if (val_len == 3)
    {
      res_int = (uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16);
    }
    if (val_len == 4)
    {
      res_int = (uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
    }
    if (pr)
    {
      Serial.print(F(" "));
      Serial.println(res_int, HEX);
    }
    return res_int;
  }
  float GetFloatValue()
  {
    float res;
    uint32_t int_res = this->GetIntValue(0x00);
    memcpy(&res, &int_res, 4);
    return res;
  }
  void SetFloatValue(float new_value)
  {
    uint32_t int_res;
    memcpy(&int_res, &new_value, 4);
    this->SetValue(int_res);
  }
};

attribute empty_res_attr = attribute(0, 0, 0, 0, 0x01);

class Cluster
{
private:
  attribute *attributes;
  uint8_t num_attr;

public:
  uint16_t id;
  Cluster(uint16_t cl_id, attribute *attr, uint8_t num)
  {
    id = cl_id;
    attributes = attr;
    num_attr = num;
  }
  bool AttributeExists(uint16_t attr_id)
  {
    for (uint8_t i = 0; i < num_attr; i++)
    {
      if (attributes[i].id == attr_id)
      {
        return 0x01;
      }
    }
    return 0x00;
  }
  attribute *GetAttr(uint16_t attr_id)
  {
    for (uint8_t i = 0; i < num_attr; i++)
    {
      if (attributes[i].id == attr_id)
      {
        return &attributes[i];
      }
    }
    Serial.print(F("Attr Not Found: "));
    Serial.println(attr_id, HEX);
    return &empty_res_attr;
  }
};

class LocalMac
{
private:
  uint8_t addr;

public:
  void Set(XBeeAddress64 mac)
  {
    EEPROM.put(addr, mac);
  }

  XBeeAddress64 Get()
  {
    XBeeAddress64 mac;
    EEPROM.get(addr, mac);
    return mac;
  }
  LocalMac(uint8_t mem_loc = 0)
  {
    addr = mem_loc;
  }
};

class Endpoint
{
private:
  uint8_t num_in_clusters;
  uint8_t num_out_clusters;

  Cluster *out_clusters;
  uint16_t dev_type;

public:
  uint8_t id;
  Cluster *in_clusters;

public:
  Endpoint(uint8_t ep_id = 0, uint16_t type_dev = 0, Cluster *in_cls = {}, Cluster *out_cls = {}, uint8_t num_in_cls = 0, uint8_t num_out_cls = 0)
  {
    id = ep_id;
    dev_type = type_dev;
    num_in_clusters = num_in_cls;
    num_out_clusters = num_out_cls;
    in_clusters = in_cls;
    out_clusters = out_cls;
  }
  bool ClusterExists(uint16_t cl_id)
  {
    for (uint8_t i = 0; i < num_in_clusters; i++)
    {
      if (cl_id == in_clusters[i].id)
      {
        return 0x01;
      }
    }
    Serial.print(F("No Cl "));
    Serial.println(cl_id, HEX);
    return 0x00;
  }
  Cluster GetCluster(uint16_t cl_id)
  {
    for (uint8_t i = 0; i < num_in_clusters; i++)
    {
      if (cl_id == in_clusters[i].id)
      {
        return in_clusters[i];
      }
    }
    Serial.print(F("No Cl "));
    Serial.println(cl_id, HEX);
    return in_clusters[0]; // Also probably a mistake
  }
  void FillInCluster(uint8_t *buffer, uint8_t buf_start)
  {
    // Fill cluster ids in to a buffer, in reverse endian
    for (uint8_t i = 0; i < num_in_clusters; i++)
    {
      memcpy(buffer + buf_start + (i * 2), &in_clusters[i].id, 2);
    }
  }
  void FillOutCluster(uint8_t *buffer, uint8_t buf_start)
  {
    // Fill cluster ids in to a buffer, in reverse endian
    for (uint8_t i = 0; i < num_out_clusters; i++)
    {
      memcpy(buffer + buf_start + (i * 2), &out_clusters[i].id, 2);
    }
  }
  void GetInClusters(uint16_t *in_cl)
  {
    for (uint8_t i = 0; i < num_in_clusters; i++)
    {
      *(in_cl + i) = in_clusters[i].id;
    }
  }
  void GetOutClusters(uint16_t *out_cl)
  {
    for (uint8_t i = 0; i < num_out_clusters; i++)
    {
      *(out_cl + i) = out_clusters[i].id;
    }
  }
  uint8_t GetNumInClusters()
  {
    return num_in_clusters;
  }
  uint8_t GetNumOutClusters()
  {
    return num_out_clusters;
  }
  uint16_t GetDevType()
  {
    return dev_type;
  }
};

// ieee high
constexpr uint8_t shCmd[] = {'S', 'H'};
// ieee low
constexpr uint8_t slCmd[] = {'S', 'L'};
// association status
constexpr uint8_t assocCmd[] = {'A', 'I'};
// panID
constexpr uint8_t netCmd[] = {'M', 'Y'};

XBeeAddress64 COORDINATOR64 = XBeeAddress64(0, 0);