/**
******************************************************************************
*
* @file       uas_types.cpp
* @brief      APM 任务航点数据结构实现：AP_WayPoint/AP_WPArray 与 .wp 任务文件读写
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "uas_types.h"


using namespace std;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static string trim(const string &s)   ///< static：库内部工具，不导出符号
{
    string              delims = " \t\n\r",
                        r;
    string::size_type   i, j;

    i = s.find_first_not_of(delims);
    j = s.find_last_not_of(delims);

    if( i == string::npos ) {
        r = "";
        return r;
    }

    if( j == string::npos ) {
        r = "";
        return r;
    }

    r = s.substr(i, j-i+1);
    return r;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void AP_WayPoint::init(void)
{
    idx = -1;

    cmd = 16;

    lat = -9999;
    lng = -9999;
    alt = 50;

    heading = 0;

    param1 = 0;
    param2 = 0;
    param3 = 0;
    param4 = 0;

    current = 0;

    frame = 0;
    autocontinue = 1;
}

void AP_WayPoint::release(void)
{
    return;
}

void AP_WayPoint::set(double lat_, double lng_, double alt_)
{
    lat = lat_;
    lng = lng_;
    alt = alt_;
}

void AP_WayPoint::setPos(double lat_, double lng_)
{
    lat = lat_;
    lng = lng_;
}

void AP_WayPoint::setAlt(double alt_)
{
    alt = alt_;
}

void AP_WayPoint::setHeading(double heading_)
{
    heading = heading_;
}

void AP_WayPoint::get(double &lat_, double &lng_, double &alt_)
{
    lat_ = lat;
    lng_ = lng;
    alt_ = alt;
}

void AP_WayPoint::getPos(double &lat_, double &lng_)
{
    lat_ = lat;
    lng_ = lng;
}

void AP_WayPoint::getAlt(double &alt_)
{
    alt_ = alt;
}

void AP_WayPoint::getHeading(double &heading_)
{
    heading_ = heading;
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

AP_WPArray::AP_WPArray()
{
    init();
}

AP_WPArray::~AP_WPArray()
{
    release();
}

void AP_WPArray::init(void)
{
    m_arrWP.clear();

    m_nWP = 0;
    m_iRW = 0;
    m_status = 0;
}

void AP_WPArray::release(void)
{
    if( m_arrWP.size() > 0 ) {
        AP_WayPointMap::iterator it;
        AP_WayPoint *p;

        for(it=m_arrWP.begin(); it!=m_arrWP.end(); it++) {
            p = it->second;
            delete p;
            p = NULL;
        }

        m_arrWP.clear();
    }

    m_nWP = 0;
    m_iRW = 0;
    m_status = 0;
}

int AP_WPArray::set(AP_WPArray *wpa)
{
    // clear old wps
    clear();

    // insert all wps
    AP_WayPointMap::iterator it;
    AP_WayPoint *p;

    for(it=wpa->m_arrWP.begin(); it!=wpa->m_arrWP.end(); it++) {
        p = it->second;

        set(*p);
    }

    return 0;
}

int AP_WPArray::set(AP_WayPoint &wp)
{
    AP_WayPointMap::iterator it;

    it = m_arrWP.find(wp.idx);
    if( it != m_arrWP.end() ) {
        *(it->second) = wp;
    } else {
        AP_WayPoint *p = new AP_WayPoint;
        *p = wp;

        m_arrWP.insert(std::pair<int, AP_WayPoint*>(wp.idx, p));
    }

    return 0;
}

int AP_WPArray::get(AP_WayPoint &wp)
{
    AP_WayPointMap::iterator it;

    it = m_arrWP.find(wp.idx);
    if( it != m_arrWP.end() ) {
        wp = *(it->second);
    } else {
        return -1;
    }

    return 0;
}

AP_WayPoint* AP_WPArray::get(int idx)
{
    AP_WayPointMap::iterator it;
    AP_WayPoint *wp;

    it = m_arrWP.find(idx);
    if( it != m_arrWP.end() ) {
        wp = it->second;
    } else {
        wp = NULL;
    }

    return wp;
}

int AP_WPArray::getAll(AP_WayPointVector &wps)
{
    AP_WayPointMap::iterator it;

    wps.clear();

    for(it=m_arrWP.begin(); it!=m_arrWP.end(); it++) {
        wps.push_back(it->second);
    }

    return 0;
}

AP_WayPointMap* AP_WPArray::getAll(void)
{
    return &m_arrWP;
}

int AP_WPArray::size(void)
{
    return m_arrWP.size();
}

int AP_WPArray::remove(int idxWP)
{
    AP_WayPointMap::iterator it;

    it = m_arrWP.find(idxWP);
    if( it != m_arrWP.end() ) {
        m_arrWP.erase(it);
    } else {
        return -1;
    }

    return 0;
}

int AP_WPArray::clear(void)
{
    release();

    return 0;
}

int AP_WPArray::save(std::string fname)
{
    FILE                        *fp = NULL;
    AP_WayPointMap::iterator    it;
    AP_WayPoint                 *p;

    // open file
    fp = fopen(fname.c_str(), "wt");
    if( fp == NULL ) {
        printf("Cannot open file: %s", fname.c_str());
        return -1;
    }

    // output waypoints
    fprintf(fp, "#waypoint number\n");
    fprintf(fp, "%d\n", (int)m_arrWP.size());

    fprintf(fp, "#waypoints list\n");
    fprintf(fp, "# idx              lat                   lng                  alt         heading\n");

    for(it=m_arrWP.begin(); it!=m_arrWP.end(); it++) {
        p = it->second;

        fprintf(fp, "%4d %24.16f %24.16f %12.3f %12.3f\n",
                p->idx,
                p->lat, p->lng, p->alt,
                p->heading);
    }

    // close file
    fclose(fp);

    return 0;
}

int AP_WPArray::load(std::string fname)
{
    FILE                        *fp = NULL;

    char                        *buf;
    std::string                 _b;
    int                         max_line_size;
    int                         s, idx, n;
    int                         i1;
    double                      lat, lng, alt, heading;

    // open file
    fp = fopen(fname.c_str(), "rt");
    if( fp == NULL ) {
        printf("Cannot open file: %s", fname.c_str());
        return -1;
    }

    // free old contents
    clear();

    // alloc memory buffer
    max_line_size = 1024;
    buf = new char[max_line_size];

    s = 0;
    idx = 0;

    while(!feof(fp)) {
        // read a line
        if( NULL == fgets(buf, max_line_size, fp) )
            break;

        // remove blank & CR
        _b = trim(buf);

        if( _b.size() < 1 )
            continue;

        // skip comment
        if( _b[0] == '#' || _b[0] == ':' )
            continue;

        if( s == 0 ) {
            // read wp number
            sscanf(_b.c_str(), "%d", &n);
            s = 1;
        } else {
            // read wp item
            sscanf(_b.c_str(), "%d %lf %lf %lf %lf",
                   &i1,
                   &lat, &lng, &alt, &heading);

            AP_WayPoint w;

            w.idx = i1;
            w.lat = lat;
            w.lng = lng;
            w.alt = alt;
            w.heading = heading;

            // insert to map
            set(w);

            idx ++;
        }
    }

    // free buffer
    delete [] buf;

    // close file
    fclose(fp);

    return 0;
}
