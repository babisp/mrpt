/* +---------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)               |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2016, Individual contributors, see AUTHORS file        |
   | See: http://www.mrpt.org/Authors - All rights reserved.                   |
   | Released under BSD License. See details in http://www.mrpt.org/License    |
   +---------------------------------------------------------------------------+ */


#include <mrpt/obs/CRawlog.h>
#include <mrpt/gui/CDisplayWindow3D.h>
#include <mrpt/maps/COccupancyGridMap2D.h>
#include <mrpt/system/filesystem.h>
#include <mrpt/system/os.h>
#include <mrpt/utils/CLoadableOptions.h>
#include <mrpt/utils/CImage.h>
#include <mrpt/utils/COutputLogger.h>
#include <mrpt/opengl/CPlanarLaserScan.h>
#include <mrpt/poses/CPoses2DSequence.h>
#include <mrpt/poses/CPosePDF.h>
#include <mrpt/graphs/CNetworkOfPoses.h>
#include <mrpt/system/string_utils.h>

#include <mrpt/graphslam/CGraphSlamEngine.h>
#include <mrpt/graphslam/apps_related/TUserOptionsChecker.h>
#include <mrpt/graphslam/apps_related/CGraphSlamHandler.h>

#include <mrpt/otherlibs/tclap/CmdLine.h>

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <cerrno>

using namespace mrpt;
using namespace mrpt::utils;
using namespace mrpt::poses;
using namespace mrpt::obs;
using namespace mrpt::system;
using namespace mrpt::graphs;
using namespace mrpt::math;
using namespace mrpt::maps;
using namespace mrpt::opengl;
using namespace mrpt::utils;
using namespace mrpt::graphslam;
using namespace mrpt::graphslam::deciders;
using namespace mrpt::graphslam::optimizers;
using namespace mrpt::graphslam::detail;

using namespace std;

// command line arguments
// ////////////////////////////////////////////////////////////

TCLAP::CmdLine cmd_line(/*output message = */ " graphslam-engine - Part of the MRPT\n",
		/* delimeter = */ ' ', /* version = */ MRPT_getVersion().c_str());

TCLAP::ValueArg<string> arg_ini_file(/*flag = */ "i", /*name = */ "ini-file",
		/*desc = */ ".ini configuration file", /* required = */ true,
		/* default value = */ "", /*typeDesc = */ "config.ini", /*parser = */ cmd_line);

TCLAP::ValueArg<string> arg_rawlog_file("r", "rawlog",
		"Rawlog dataset file",	true, "", "contents.rawlog", /*parser = */ cmd_line);


TCLAP::ValueArg<string> arg_ground_truth_file("g", "ground-truth",
		"Ground-truth textfile",	false, "", "contents.rawlog.GT.txt", cmd_line);

// Specify the Registration Deciders to use
TCLAP::ValueArg<string> arg_node_reg("n"  , "node-reg"  , "Specify Node Registration Decider" , false , "CFixedIntervalsNRD" , "CICPCriteriaNRD" , cmd_line);
TCLAP::ValueArg<string> arg_edge_reg("e"  , "edge-reg"  , "Specify Edge Registration Decider" , false , "CICPCriteriaERD"    , "CICPCriteriaERD" , cmd_line);
TCLAP::ValueArg<string> arg_optimizer("o" , "optimizer" , "Specify GraphSlam Optimizer"       , false , "CLevMarqGSO"        , "CLevMarqGSO"     , cmd_line);

// list available deciders
TCLAP::SwitchArg list_node_registrars("" , "list-node-regs"  , "List available node registration decider classes"  , cmd_line , false);
TCLAP::SwitchArg list_edge_registrars("" , "list-edge-regs"  , "List available edge registration decider classes"  , cmd_line , false);
TCLAP::SwitchArg list_all_registrars(""  , "list-regs"       , "List (all) available registration decider classes" , cmd_line , false);
TCLAP::SwitchArg list_optimizers(""      , "list-optimizers" , "List (all) available graphslam optimizer classes"  , cmd_line , false);

// specify whether to run on headless mode - no visuals
// flag overrides all visualization related directives of the .ini file
// handy for usage when no visualization is needed or when running on
// real-robots in headless mode
TCLAP::SwitchArg disable_visuals("","disable-visuals","Disable Visualization - Overrides related visualize* directives of the .ini file",cmd_line, false);

TCLAP::SwitchArg dim_2d("" , "2d"  , "Construct 2D graph of constraints (use CPosePDFGaussianInf)");
TCLAP::SwitchArg dim_3d("" , "3d"  , "Construct 3D graph of constraints (use CPose3DPDFGaussianInf)");

// TODO - format whole thing into a class.
/**\brief make instance of CGraphSlamEngine and execute  graphSLAM
 */
template<class GRAPH_t>
bool execGraphSlamEngine(mrpt::utils::COutputLogger* logger) {

	// Instance for managing the available graphslam deciders optimizers
	TUserOptionsChecker<GRAPH_t> options_checker;
	options_checker.createDeciderOptimizerMappings();
	options_checker.populateDeciderOptimizerProperties();

	// fetch the command line options
	// ////////////////////////////////////////////////////////////

	// decide whether to display the help messages for the deciders/optimizers
	{
		bool list_registrars = false;

		if (list_all_registrars.getValue()) {
			options_checker.dumpRegistrarsToConsole("all");
			list_registrars = true;
		}
		if (list_node_registrars.getValue()) {
			options_checker.dumpRegistrarsToConsole("node");
			list_registrars = true;
		}
		if (list_edge_registrars.getValue()) {
			options_checker.dumpRegistrarsToConsole("edge");
			list_registrars = true;
		}

		if (list_optimizers.getValue()) {
			options_checker.dumpOptimizersToConsole();
		}

		if (list_registrars || list_optimizers.getValue()) {
			logger->logFmt(LVL_INFO, "Exiting.. ");
			return 0;
		}
	}

	// fetch which registration deciders / optimizer to use
	string node_reg = arg_node_reg.getValue();
	string edge_reg = arg_edge_reg.getValue();
	string optimizer = arg_optimizer.getValue();
	ASSERTMSG_(options_checker.checkRegistrationDeciderExists(node_reg, "node"),
			format("\nNode Registration Decider %s is not available.\n",
				node_reg.c_str()) );
	ASSERTMSG_(options_checker.checkRegistrationDeciderExists(edge_reg, "edge"),
			format("\nEdge Registration Decider %s is not available.\n",
				edge_reg.c_str()) );
	ASSERTMSG_(options_checker.checkOptimizerExists(optimizer),
			format("\nOptimizer %s is not available\n",
				optimizer.c_str()) );

	// fetch the filenames
	// ini file
	string ini_fname = arg_ini_file.getValue();
	// rawlog file
	string rawlog_fname = arg_rawlog_file.getValue();

	// ground-truth file
	string ground_truth_fname;
	if ( arg_ground_truth_file.isSet() ) {
		ground_truth_fname = arg_ground_truth_file.getValue();
	}

	if (disable_visuals.getValue()) { // enabling Visualization objects
		logger->logFmt(LVL_WARN, "Running on headless mode - Visuals disabled");
	}

	logger->logFmt(LVL_INFO, "Node registration decider: %s", node_reg.c_str());
	logger->logFmt(LVL_INFO, "Edge registration decider: %s", edge_reg.c_str());
	logger->logFmt(LVL_INFO, "graphSLAM Optimizer: %s", optimizer.c_str());

	// CGraphSlamHandler initialization
	CGraphSlamHandler graphslam_handler;
	graphslam_handler.setOutputLoggerPtr(logger);
	graphslam_handler.readConfigFname(ini_fname);
	graphslam_handler.setRawlogFname(rawlog_fname);

	// Visuals initialization
	if (!disable_visuals.getValue()) {
		graphslam_handler.initVisualization();
	}

	CGraphSlamEngine<GRAPH_t> graphslam_engine(
			ini_fname,
			rawlog_fname,
			ground_truth_fname,
			graphslam_handler.win_manager,
			options_checker.node_regs_map[node_reg](),
			options_checker.edge_regs_map[edge_reg](),
			options_checker.optimizers_map[optimizer]());

	// print the problem parameters
	graphslam_handler.printParams();
	graphslam_engine.printParams();

	// Variables initialization
	CFileGZInputStream rawlog_stream(rawlog_fname);
	CActionCollectionPtr action;
	CSensoryFramePtr observations;
	CObservationPtr observation;
	size_t curr_rawlog_entry;

	// Read the dataset and pass the measurements to CGraphSlamEngine
	bool cont_exec = true;
	while (CRawlog::getActionObservationPairOrObservation(
				rawlog_stream,
				action,
				observations,
				observation,
				curr_rawlog_entry) && cont_exec) {

		// actual call to the graphSLAM execution method
		// Exit if user pressed C-c
		cont_exec = graphslam_engine.execGraphSlamStep(
				action,
				observations,
				observation,
				curr_rawlog_entry);

	}
	logger->logFmt(LVL_WARN, "Finished graphslam execution.");

	//
	// Postprocessing
	//

	logger->logFmt(LVL_INFO, "Generating overall report...");
	graphslam_engine.generateReportFiles(graphslam_handler.output_dir_fname);
	// save the graph and the 3DScene 
	if (graphslam_handler.save_graph) {
		std::string save_graph_fname = 
			graphslam_handler.output_dir_fname +
			"/" +
			graphslam_handler.save_graph_fname;
		graphslam_engine.saveGraph(&save_graph_fname);
	}
	if (!disable_visuals.getValue() && graphslam_handler.save_3DScene) {
		std::string save_3DScene_fname = 
			graphslam_handler.output_dir_fname +
			"/" +
			graphslam_handler.save_3DScene_fname;

		graphslam_engine.save3DScene(&save_3DScene_fname);
	}

	// get the occupancy map that was built
	if (graphslam_handler.save_map) {
		COccupancyGridMap2DPtr map = mrpt::maps::COccupancyGridMap2D::Create();
		graphslam_engine.getMap(map);
		map->saveMetricMapRepresentationToFile(
				graphslam_handler.output_dir_fname +
				"/" +
				graphslam_handler.save_map_fname);
	}

	return 0;
}

// Main
// ////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	// initializign the logger instance
	COutputLogger logger("graphslam-engine_app");
	logger.logging_enable_keep_record = true;

	try {

		bool showHelp		 = argc>1 && !os::_strcmp(argv[1],"--help");
		bool showVersion = argc>1 && !os::_strcmp(argv[1],"--version");

		// Input Validation
		cmd_line.xorAdd(dim_2d, dim_3d);
		if (!cmd_line.parse( argc, argv ) ||  showVersion || showHelp) {
			return 0;
		}

		// CGraphSlamEngine initialization
		bool success = true;
		if (dim_2d.getValue()) {
			success = execGraphSlamEngine<CNetworkOfPoses2DInf>(&logger);
		}
		else {
			success = execGraphSlamEngine<CNetworkOfPoses3DInf>(&logger);
		}

		return success;
	}
	catch (exception& e) {
		logger.logFmt(LVL_ERROR, "Program finished due to an exception!!\n%s\n",
				e.what());
		printf("%s", e.what());

		mrpt::system::pause();
		return -1;
	}
	catch (...) {
		logger.logFmt(LVL_ERROR, "Program finished due to an untyped exception!!");
		mrpt::system::pause();
		return -1;
	}

	return 0;
}


