/* +---------------------------------------------------------------------------+
	 |                     Mobile Robot Programming Toolkit (MRPT)               |
	 |                          http://www.mrpt.org/                             |
	 |                                                                           |
	 | Copyright (c) 2005-2016, Individual contributors, see AUTHORS file        |
	 | See: http://www.mrpt.org/Authors - All rights reserved.                   |
	 | Released under BSD License. See details in http://www.mrpt.org/License    |
	 +---------------------------------------------------------------------------+ */

#ifndef CFIXEDINTERVALSNRD_IMPL_H
#define CFIXEDINTERVALSNRD_IMPL_H

namespace mrpt { namespace graphslam { namespace deciders {

// Ctors, Dtors
//////////////////////////////////////////////////////////////

template<class GRAPH_t>
CFixedIntervalsNRD<GRAPH_t>::CFixedIntervalsNRD() {
	this->initCFixedIntervalsNRD();
}
template<class GRAPH_t>
void CFixedIntervalsNRD<GRAPH_t>::initCFixedIntervalsNRD() {
	using namespace mrpt::utils;
	this->initializeLoggers("CFixedIntervalsNRD");

	m_since_prev_node_PDF.cov_inv = this->m_init_inf_mat;
	m_since_prev_node_PDF.mean = pose_t();

	this->logFmt(LVL_DEBUG, "IntervalsNRD: Initialized class object");
}
template<class GRAPH_t>
CFixedIntervalsNRD<GRAPH_t>::~CFixedIntervalsNRD() { }

// Member function implementations
//////////////////////////////////////////////////////////////

template<class GRAPH_t>
typename GRAPH_t::constraint_t::type_value
CFixedIntervalsNRD<GRAPH_t>::getCurrentRobotPosEstimation() const {
	return m_curr_estimated_pose;
}

template<class GRAPH_t>
bool CFixedIntervalsNRD<GRAPH_t>::updateState(
		mrpt::obs::CActionCollectionPtr action,
		mrpt::obs::CSensoryFramePtr observations,
		mrpt::obs::CObservationPtr observation )  {
	MRPT_START;
	using namespace mrpt::obs;
	using namespace mrpt::math;
	using namespace mrpt::utils;
	using namespace mrpt::poses;

	// don't use the measurements in this implementation
	MRPT_UNUSED_PARAM(observations);

	if (observation.present()) { // FORMAT #2 - observation-only format
		m_observation_only_rawlog = true;

		if (IS_CLASS(observation, CObservationOdometry)) {

			CObservationOdometryPtr obs_odometry =
				static_cast<CObservationOdometryPtr>(observation);
			// not incremental - gives the absolute odometry reading
			m_curr_odometry_only_pose = obs_odometry->odometry;
			this->logFmt(LVL_DEBUG, "Current odometry-only pose: %s",
					m_curr_odometry_only_pose.asString().c_str());

			// I don't have any information about the covariane of the move in
			// observation-only format
			m_since_prev_node_PDF.mean =
				m_curr_odometry_only_pose - m_last_odometry_only_pose;
		}
	}
	else { // FORMAT #1 - action-observation format
		m_observation_only_rawlog = false;

		mrpt::poses::CPose3DPDFGaussian move_pdf;
		bool found = action->getFirstMovementEstimation(move_pdf);
		if (found) {
			// update the relative PDF of the path since the LAST node was inserted
			constraint_t incr_constraint;
			incr_constraint.copyFrom(move_pdf);
			m_since_prev_node_PDF += incr_constraint;
		}

		// TODO - remove these
		//if (action->getBestMovementEstimation() ) {
			//mrpt::obs::CActionRobotMovement2DPtr robot_move = action->getBestMovementEstimation();
			//mrpt::poses::CPosePDFPtr increment = robot_move->poseChange;
			//pose_t increment_pose = increment->getMeanVal();
			//inf_mat_t increment_inf_mat;
			//increment->getInformationMatrix(increment_inf_mat);

			////update the relative PDF of the path since the LAST node was inserted
			//constraint_t incr_constraint(increment_pose, increment_inf_mat);
			//m_since_prev_node_PDF += incr_constraint;
		//}

	} // ELSE - FORMAT #1

	if (this->m_prev_registered_node != INVALID_NODEID) {
		m_curr_estimated_pose = this->m_graph->nodes.at(this->m_prev_registered_node);
	}
	m_curr_estimated_pose += m_since_prev_node_PDF.getMeanVal();

	bool registered = this->checkRegistrationCondition();

	if (registered) {
		if (m_observation_only_rawlog) {
			// keep track of the odometry-only pose_t at the last inserted graph node
			m_last_odometry_only_pose = m_curr_odometry_only_pose;
		}
		m_since_prev_node_PDF = constraint_t();
		m_since_prev_node_PDF.cov_inv = this->m_init_inf_mat;
	}

	return registered;

	MRPT_END;
}

template<class GRAPH_t>
bool CFixedIntervalsNRD<GRAPH_t>::checkRegistrationCondition() {
	MRPT_START;

	// check that a node has already been registered - if not default to (0,0,0)
	pose_t last_pose_inserted = this->m_prev_registered_node != INVALID_NODEID? 
		this->m_graph->nodes.at(this->m_prev_registered_node): pose_t();

	// odometry criterion
	bool registered = false;
	if (this->checkRegistrationCondition(
				last_pose_inserted,
				m_curr_estimated_pose)) {
		registered = this->registerNewNode(m_since_prev_node_PDF);
	}

	return registered;

	MRPT_END;
}

template<class GRAPH_t>
bool CFixedIntervalsNRD<GRAPH_t>::checkRegistrationCondition(
		mrpt::poses::CPose2D p1,
		mrpt::poses::CPose2D p2) {
	using namespace mrpt::math;
	
	bool res = false;
	if ((p1.distanceTo(p2) > params.registration_max_distance) ||
			(fabs(wrapToPi(p1.phi() - p2.phi())) > params.registration_max_angle)) {
		res = true;
	}

	return res;
}

// TODO - check this
template<class GRAPH_t>
bool CFixedIntervalsNRD<GRAPH_t>::checkRegistrationCondition(
		mrpt::poses::CPose3D p1,
		mrpt::poses::CPose3D p2) {
	using namespace mrpt::math;
	
	bool res = false;
	if ((p1.distanceTo(p2) > params.registration_max_distance) ||
			(fabs(wrapToPi(p1.roll() - p2.roll())) > params.registration_max_angle) ||
			(fabs(wrapToPi(p1.pitch() - p2.pitch())) > params.registration_max_angle) ||
			(fabs(wrapToPi(p1.yaw() - p2.yaw())) > params.registration_max_angle)) {
		res = true;
	}

	return res;
}

template<class GRAPH_t>
void CFixedIntervalsNRD<GRAPH_t>::loadParams(const std::string& source_fname) {
	MRPT_START;
	using namespace mrpt::utils;
	parent::loadParams(source_fname);

	params.loadFromConfigFileName(source_fname,
			"NodeRegistrationDeciderParameters");

	// set the logging level if given by the user
	CConfigFile source(source_fname);
	int min_verbosity_level = source.read_int(
			"NodeRegistrationDeciderParameters",
			"class_verbosity",
			1, false);
	this->setMinLoggingLevel(VerbosityLevel(min_verbosity_level));


	this->logFmt(LVL_DEBUG, "Successfully loaded parameters.");

	MRPT_END;
}

template<class GRAPH_t>
void CFixedIntervalsNRD<GRAPH_t>::printParams() const {
	MRPT_START;
	parent::printParams();
	params.dumpToConsole();

	MRPT_END;
}

template<class GRAPH_t>
void CFixedIntervalsNRD<GRAPH_t>::getDescriptiveReport(std::string* report_str) const {
	MRPT_START;
	using namespace std;

	const std::string report_sep(2, '\n');
	const std::string header_sep(80, '#');

	// Report on graph
	stringstream class_props_ss;
	class_props_ss << "Strategy: " <<
		"Fixed Odometry-based Intervals" << std::endl;
	class_props_ss << header_sep << std::endl;

	// time and output logging
	const std::string time_res = this->m_time_logger.getStatsAsText();
	const std::string output_res = this->getLogAsString();

	// merge the individual reports
	report_str->clear();
	parent::getDescriptiveReport(report_str);

	*report_str += class_props_ss.str();
	*report_str += report_sep;

	// configuration parameters
	*report_str += params.getAsString();
	*report_str += report_sep;

	// loggers results
	*report_str += time_res;
	*report_str += report_sep;

	*report_str += output_res;
	*report_str += report_sep;

	MRPT_END;
}

// TParams
//////////////////////////////////////////////////////////////
template<class GRAPH_t>
CFixedIntervalsNRD<GRAPH_t>::TParams::TParams() {
}
template<class GRAPH_t>
CFixedIntervalsNRD<GRAPH_t>::TParams::~TParams() {
}
template<class GRAPH_t>
void CFixedIntervalsNRD<GRAPH_t>::TParams::dumpToTextStream(
		mrpt::utils::CStream &out) const {
	MRPT_START;
	out.printf("%s", this->getAsString().c_str());
	MRPT_END;
}
template<class GRAPH_t>
void CFixedIntervalsNRD<GRAPH_t>::TParams::loadFromConfigFile(
		const mrpt::utils::CConfigFileBase &source,
		const std::string &section) {
	MRPT_START;
	using namespace mrpt::math;
	using namespace mrpt::utils;

	registration_max_distance = source.read_double( section,
			"registration_max_distance",
			0.5 /* meter */, false);
	registration_max_angle = source.read_double( section,
			"registration_max_angle",
			60 /* degrees */, false);
	registration_max_angle = DEG2RAD(registration_max_angle);

	MRPT_END;
}

template<class GRAPH_t>
void CFixedIntervalsNRD<GRAPH_t>::TParams::getAsString(std::string* params_out) const {
	MRPT_START;
	using namespace mrpt::math;
	using namespace mrpt::utils;

	double max_angle_deg = RAD2DEG(registration_max_angle);
	params_out->clear();

	*params_out += "------------------[ Fixed Intervals Node Registration ]------------------\n";
	*params_out += mrpt::format("Max distance for registration = %.2f m\n", registration_max_distance);
	*params_out += mrpt::format("Max angle for registration    = %.2f deg\n", max_angle_deg);

	MRPT_END;
}
template<class GRAPH_t>
std::string CFixedIntervalsNRD<GRAPH_t>::TParams::getAsString() const {
	MRPT_START;

	std::string str;
	this->getAsString(&str);
	return str;

	MRPT_END;
}

} } } // end of namespaces

#endif /* end of include guard: CFIXEDINTERVALSNRD_IMPL_H */
