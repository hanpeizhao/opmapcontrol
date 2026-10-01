/**
******************************************************************************
*
* @file       uas_types.h
* @brief      APM(ArduPilot) 任务航点数据结构：.wp 任务文件的读写格式
*             （仅 waypoint_store 使用，作为库航点 ↔ APM 任务文件的转换层）
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#ifndef __UAS_TYPES_H__
#define __UAS_TYPES_H__

#include <stdio.h>
#include <stdint.h>

#include <string>
#include <vector>
#include <map>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class AP_WayPoint
{
public:
    AP_WayPoint() { init(); }
    ~AP_WayPoint() { release(); }

    void init(void);
    void release(void);

    void set(double lat_, double lng_, double alt_);
    void setPos(double lat_, double lng_);
    void setAlt(double alt_);
    void setHeading(double heading_);

    void get(double &lat_, double &lng_, double &alt_);
    void getPos(double &lat_, double &lng_);
    void getAlt(double &alt_);
    void getHeading(double &heading_);

public:
    int         idx;                    ///< index number

    int         cmd;                    ///< mission command

    double      lat, lng;               ///< latitdue & longtitude
    double      alt;                    ///< altitude
    double      heading;                ///< heading (0:North, 90: Est, 180: South, 270: West)

    float       param1;                 ///< PARAM1, see MAV_CMD enum
    float       param2;                 ///< PARAM2, see MAV_CMD enum
    float       param3;                 ///< PARAM3, see MAV_CMD enum
    float       param4;                 ///< PARAM4, see MAV_CMD enum

    int         current;                ///< current mission (1:True, 0:False)

    int         frame;                  ///< The coordinate system of the MISSION. see MAV_FRAME in mavlink_types.h
    uint8_t     autocontinue;           ///< autocontinue to next wp
};


typedef std::map<int, AP_WayPoint*>     AP_WayPointMap;
typedef std::vector<AP_WayPoint*>       AP_WayPointVector;


class AP_WPArray
{
public:
    AP_WPArray();
    ~AP_WPArray();

    void init(void);
    void release(void);

    int set(AP_WPArray *wpa);
    int set(AP_WayPoint &wp);
    int get(AP_WayPoint &wp);
    AP_WayPoint* get(int idx);
    int getAll(AP_WayPointVector &wps);
    AP_WayPointMap* getAll();

    int size(void);

    int remove(int idxWP);
    int clear(void);

    int save(std::string fname);
    int load(std::string fname);

protected:
    AP_WayPointMap      m_arrWP;        ///< waypoints map

public:
    int                 m_nWP;          ///< total waypoints
    int                 m_iRW;          ///< current waypoint
    int                 m_status;       ///< R/W status
    uint64_t            m_tLast;        ///< Last R/W timestamp (in ms)
};

#endif // end of __UAS_TYPES_H__
