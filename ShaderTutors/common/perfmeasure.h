
#ifndef _PERFMEASURE_H_
#define _PERFMEASURE_H_

#include <vector>
#include <string>
#include <cstdint>

class PerfMeasure
{
	struct Measurement {
		std::string	ID;
		int64_t		time;
		int64_t		accumtime;
		int64_t		count;

		Measurement();
	};

	typedef std::vector<Measurement> DataArray;

private:
	DataArray	data;
	size_t		counter;

public:
	PerfMeasure();

	void Measure(const std::string& id = "");
	void Dump() const;
	void Start();
};

#endif
