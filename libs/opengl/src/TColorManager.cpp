#include <mrpt/opengl/TColorManager.h>

using namespace mrpt::utils;
using namespace mrpt::opengl::graph_tools;
using namespace std;

TColorManager::TColorManager(
		bool use_standard_colors_first/* = true */):
	color_thresh(255) {

	this->reset();

	this->use_standard_colors_first = use_standard_colors_first;
}

TColorManager::~TColorManager() { }

TColor TColorManager::getNextTColor() {

	if (have_exceeded_colors) {
		// pick and return a random color triad
		return TColor(
				rand() % (255 + 1),
				rand() % (255 + 1),
				rand() % (255 + 1));
	}

	// start updating by the step if we don't use (or have already used) the
	// standard colors
	if (!use_standard_colors_first || have_used_standard_colors) {
		this->advanceRGBCounters();
	}
	else {
		bool used_red = used_colors.find(TColor::red) != used_colors.end();
		bool used_green = used_colors.find(TColor::green) != used_colors.end();
		bool used_blue = used_colors.find(TColor::blue) != used_colors.end();

		// fixed order of usage
		// red -> green -> blue
		if (!used_red) {
			curr_color = TColor::red;
		}
		else if (!used_green) {
			curr_color = TColor::green;
		}
		else if (!used_blue) {
			curr_color = TColor::blue;
			have_used_standard_colors = true;
		}
	}

	this->markColorAsUsed(curr_color);
	return curr_color;
}

TColorf TColorManager::getNextTColorf() {
	return TColorf(this->getNextTColor());
}

void TColorManager::advanceRGBCounters() {
	// method is used only when we either don't use or have already used the
	// standard colors.
	ASSERT_(
			!use_standard_colors_first ||
			have_used_standard_colors);

	THROW_EXCEPTION("Not yet implemented.");
	// if standard colors have already been used then at first color is TColor::blue
	if (curr_color == TColor::blue) {
		curr_color = TColor();

		color_step_triad.B = color_step;
	}

	// first advance blue until we reach the limit 255
	// then advance green until we reach the limit 255
	// finally advance red until we reach the limit 255

	// TODO - how do i do this?
	curr_color += color_step_triad;


	// if the standard colors are to be used first make sure that the color we
	// end up on is not a standard one.
	if (use_standard_colors_first &&
			(curr_color == TColor::red ||
			 curr_color == TColor::green ||
			 curr_color == TColor::blue)) {
		this->advanceRGBCounters();
	}

}

void TColorManager::markColorAsUsed(TColor color) {

	pair<set<TColor>::const_iterator, bool> ret =
		used_colors.insert(color);

	// this should always return True as this shouldn't exist prior to this call
	ASSERT_(ret.second);
}

void TColorManager::reset() {
	used_colors.clear();

	curr_color = TColor();
	color_step_triad = TColor(0, 0, 50);

	have_exceeded_colors = false;
	use_standard_colors_first = true;
	have_used_standard_colors = false;
}

bool TColorManager::checkStdColorsUsed() {
	bool ret =
		used_colors.find(TColor::red) != used_colors.end() &&
		used_colors.find(TColor::green) != used_colors.end() &&
		used_colors.find(TColor::blue) != used_colors.end();

	return ret;
}
