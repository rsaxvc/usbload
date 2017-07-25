#include <iostream>
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/

#include "devreader-linux-proc.h"
#include "stringutils.h"

#include <fstream>
#include <list>
#include <string>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

DevReaderLinuxProc::DevReaderLinuxProc(const string& deviceName)
    : DevReader(deviceName)
{
}

DevReaderLinuxProc::~DevReaderLinuxProc()
{
}

bool DevReaderLinuxProc::isAvailable()
{
    struct stat procStat;
    if(stat("/proc/usbstats", &procStat) < 0 || ! S_ISREG(procStat.st_mode))
        return false;

    return true;
}

struct diskstats
{
    unsigned long bus;
    unsigned long dev;
	unsigned long long stats_packets[2];
	unsigned long long stats_bytes[2];
    string name;

    bool parse(std::string input)
    {
        // read device data
		unsigned long long temp;
        istringstream sin(input);

        sin
            >> bus
            >> dev;

        for( unsigned i = 0; i < 2; ++i )
        {
	        stats_bytes[i] = 0;
            for( unsigned j = 0; j < 4; ++j )
            {
				sin >> temp;
				stats_bytes[i] += temp;
            }
        }

        for( unsigned i = 0; i < 2; ++i )
        {
            stats_packets[i] = 0;
            for( unsigned j = 0; j < 4; ++j )
            {
                sin >> temp;
                stats_packets[i] += temp;
            }
        }

        std::ostringstream out;
        out << bus << "-" << dev;
        name = out.str();

        return !sin.fail();
    }
};

list<string> DevReaderLinuxProc::findAllDevices()
{
    list<string> interfaceNames;

    ifstream fin("/proc/usbstats");
    if(!fin.is_open())
        return interfaceNames;

    // read all remaining lines and extract the device name
    while(fin.good())
    {
        string line;
        getline(fin, line);
        diskstats d;
        if( !d.parse( line ) )
            continue;

        interfaceNames.push_back(d.name);
    }

    return interfaceNames;
}

void DevReaderLinuxProc::readFromDevice(DataFrame& dataFrame)
{
    if(m_deviceName.empty())
        return;

    ifstream fin("/proc/usbstats");
    if(!fin.is_open())
        return;

    // search for device
    while(fin.good())
    {
        string line;
        getline(fin, line);

        diskstats d;
        // read device data
        if( !d.parse( line ) )
            break;

        // check if it is the device we want
        if(m_deviceName != d.name)
            continue;

        dataFrame.setTotalDataIn(d.stats_bytes[0]);
        dataFrame.setTotalDataOut(d.stats_bytes[1]);

        dataFrame.setTotalPacketsIn(d.stats_packets[0]);
        dataFrame.setTotalPacketsOut(d.stats_packets[1]);

        dataFrame.setTotalErrorsIn(0);
        dataFrame.setTotalErrorsOut(0);

        dataFrame.setTotalDropsIn(0);
        dataFrame.setTotalDropsOut(0);

        dataFrame.setValid(true);

        break;
    }
}

