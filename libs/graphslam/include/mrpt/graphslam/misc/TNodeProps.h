/* +---------------------------------------------------------------------------+
	 |                     Mobile Robot Programming Toolkit (MRPT)               |
	 |                          http://www.mrpt.org/                             |
	 |                                                                           |
	 | Copyright (c) 2005-2016, Individual contributors, see AUTHORS file        |
	 | See: http://www.mrpt.org/Authors - All rights reserved.                   |
	 | Released under BSD License. See details in http://www.mrpt.org/License    |
	 +---------------------------------------------------------------------------+ */
#ifndef TNODEPROPS_H
#define TNODEPROPS_H

#include <mrpt/obs/CObservation2DRangeScan.h>
#include <mrpt/obs/CObservation3DRangeScan.h>
#include <string>

namespace mrpt { namespace graphslam { namespace detail {

template<class GRAPH_T>
struct TNodeProps {
	typename GRAPH_T::global_pose_t pose;
	mrpt::obs::CObservation2DRangeScanPtr scan2D;
	mrpt::obs::CObservation3DRangeScanPtr scan3D;

	TNodeProps operator=(const TNodeProps& other) {
		this->pose = other.pose;
		this->scan2D = other.scan2D;
		this->scan3D = other.scan3D;
		return *this;
	}

	void getAsString(std::string* str) const {
		ASSERT_(str);
		str->clear();
		*str += mrpt::format("Pose: %s|\t", this->pose.asString().c_str());
		if (this->scan2D.present()) {
			*str += mrpt::format("Scan2D #%lu", this->scan2D->getScanSize());
		}
		else {
			*str += "Scan2D: NONE";
		}
		// TODO CObservation3DRangeScan does not implement getScanSize()
		// *str += "\t";
		// if (this->scan3D.present()) {
		// 	*str += mrpt::format("Scan3D #%lu", this->scan3D->getScanSize());
		// }
		// else {
		// 	*str += "Scan3D: NONE";
		// }
		*str += "\n";
	}
	std::string getAsString() const {
		std::string str;
		this->getAsString(&str);
		return str;
	}

	friend std::ostream& operator<<(std::ostream& o, const TNodeProps& obj) {
		o << obj.getAsString() << std::endl;
		return o;
	}
};

} } } // end of namespaces

#endif /* end of include guard: TNODEPROPS_H */
