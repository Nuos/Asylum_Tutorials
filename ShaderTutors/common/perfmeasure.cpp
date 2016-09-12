
#include "perfmeasure.h"

#include <windows.h>
#include <iostream>

static LARGE_INTEGER qwTime;
static LARGE_INTEGER qwTicksPerSec = { { 0, 0 } };

PerfMeasure::Measurement::Measurement()
{
	time = 0;
	accumtime = 0;
	count = 0;
}

PerfMeasure::PerfMeasure()
{
	counter = 0;

	data.reserve(10);
	data.resize(1);

	QueryPerformanceFrequency(&qwTicksPerSec);
}

void PerfMeasure::Measure(const std::string& id)
{
	if( counter >= data.size() )
	{
		data.resize(counter + 1);
		data.back().ID = id;
	}

	Measurement& msm = data[counter];
	const Measurement& prev = data[counter - 1];

	QueryPerformanceCounter(&qwTime);
	
	msm.time = qwTime.QuadPart;
	msm.accumtime += (qwTime.QuadPart - prev.time);

	++msm.count;
	++counter;
}

void PerfMeasure::Dump() const
{
	double time;

	std::cout << "\n";

	for( size_t i = 1; i < data.size(); ++i )
	{
		if( data[i].ID.size() > 0 )
		{
			time = (double)data[i].accumtime / (double)qwTicksPerSec.QuadPart;
			std::cout << data[i].ID << ": " << time << " s (" << data[i].count << ")\n";
		}
	}

	std::cout << std::endl;
}

void PerfMeasure::Start()
{
	counter = 1;

	QueryPerformanceCounter(&qwTime);
	data[0].time = qwTime.QuadPart;
}
