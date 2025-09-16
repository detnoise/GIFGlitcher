#include "plugin.hpp"
#include "GIFGlitcher.hpp"

Plugin* pluginInstance;
Model* modelGIFGlitcher;

void init(Plugin* p) {
	pluginInstance = p;

	modelGIFGlitcher = createModel<GIFGlitcher, GIFGlitcherWidget>("GIFGlitcher");
	
	p->addModel(modelGIFGlitcher);
}
