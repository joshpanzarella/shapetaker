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
    p->addModel(modelSpecula);
    p->addModel(modelChimera);
    p->addModel(modelTorsion);
    p->addModel(modelTessellation);
    p->addModel(modelPatina);
    p->addModel(modelReverie);
    p->addModel(modelUtilityPanel);
    p->addModel(modelNocturneTV);
}
