#include <stdint.h>
#include <EEPROM.h>
#include <XBee.h>

#define UKN_NET_ADDR 0xFFFE
#define HA_PROFILE_ID 0x0104

#define MATCH_DESC_RQST 0x0006
#define MATCH_DESC_RSP 0x8006
#define SIMPLE_DESC_RSP 0x8004
#define ACTIVE_EP_RSP 0x8005
#define ACTIVE_EP_RQST 0x0005
#define SIMPLE_DESC_RQST 0x0004
#define READ_ATTRIBUTES 0x0000

// Input
#define BASIC_CLUSTER_ID 0x0000
#define IDENTIFY_CLUSTER_ID 0x0003
#define GROUPS_CLUSTER_ID 0x0004
#define SCENES_CLUSTER_ID 0x0005
#define ON_OFF_CLUSTER_ID 0x0006
#define LEVEL_CONTROL_CLUSTER_ID 0x0008
#define LIGHT_LINK_CLUSTER_ID 0x1000
#define TEMP_CLUSTER_ID 0x0402
#define HUMIDITY_CLUSTER_ID 0x405
#define BINARY_INPUT_CLUSTER_ID 0x000f
#define IAS_ZONE_CLUSTER_ID 0x0500
#define METERING_CLUSTER_ID 0x0702
#define COLOR_CLUSTER_ID 0x0300

// Attr id
#define INSTANTANEOUS_DEMAND 0x0400

// Output
#define OTA_CLUSTER_ID 0x0019 // Upgrade

// Data Types
#define ZCL_INT16_T 0x09
#define ZCL_CHAR_STR 0x42
#define ZCL_UINT8_T 0x20
#define ZCL_UINT16_T 0x21
#define ZCL_BOOL 0x10
#define ZCL_ENUM8 0x30
#define ZCL_ENUM16 0x31
#define ZCL_MAP8 0x18
#define ZCL_MAP16 0x19

// Device
#define ON_OFF_LIGHT 0x0100
#define DIMMABLE_LIGHT 0x0101
#define COLOR_LIGHT 0x0102
#define TEMPERATURE_SENSOR 0x0302
#define ON_OFF_OUTPUT 0x0002
#define IAS_ZONE 0x0402
#define ON_OFF_SENSOR 0x0850

// Attributes
#define ATTR_CURRENT_X 0x0002
#define ATTR_CURRENT_Y 0x0003
#define ATTR_CURRENT_CT_MRDS 0x0006

// Define Steps
#define START 0
#define ASSOCIATE 1
#define NWK 2
#define CFG_CMP 3
#define DEV_ANN 4
#define READY 5

class attribute
{
public:
  uint16_t id;
  uint8_t *value;
  uint8_t val_len;
  uint8_t type;
  attribute(uint16_t a_id, uint8_t *a_value, uint8_t a_val_len, uint8_t a_type)
  {
    id = a_id;
    value = new uint8_t[a_val_len];
    memcpy(value, a_value, a_val_len);
    val_len = a_val_len;
    type = a_type;
  }

  void SetValue(uint32_t new_value)
  {
    for (uint8_t i = 0; i < val_len; i++)
    {
      value[i] = ((uint32_t)new_value >> (i * 8)) & 0xff;
    }
  }

  uint32_t GetIntValue()
  {
    if (val_len == 1)
    {
      return (uint32_t)value[0];
    }
    if (val_len == 2)
    {
      return (uint32_t)value[0] | ((uint32_t)value[1] << 8);
    }
    if (val_len == 3)
    {
      return (uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16);
    }
    if (val_len == 4)
    {
      return (uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
    }
  }
};

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
    return &attribute{0, 0, 0, 0};
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
    Serial.println(cl_id);
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
static const uint8_t shCmd[] = {'S', 'H'};
// ieee low
static const uint8_t slCmd[] = {'S', 'L'};
// association status
static const uint8_t assocCmd[] = {'A', 'I'};
// panID
static const uint8_t netCmd[] = {'M', 'Y'};

XBeeAddress64 COORDINATOR64 = XBeeAddress64(0, 0);