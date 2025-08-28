#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
    pluginInstance = p;
    
    p->addModel(modelClairaudient);
    p->addModel(modelChiaroscuro);
    p->addModel(modelFatebinder);
    p->addModel(modelInvolution);
    p->addModel(modelEvocation);
    p->addModel(modelIncantation);
    p->addModel(modelTransmutation);
}