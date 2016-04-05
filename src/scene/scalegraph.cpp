/****************************************************************************************
*                                                                                       *
*                                                                                       *
*                                  This is MINE,                                        *
*                                                                                       *
*                                  ALL MINE!!!!                                         *
*                                                                                       *
*                                                                                       *
*                                                                                       *
*                                                                                       *
****************************************************************************************/

#include <openspace/scene/scalegraph.h>
#include <openspace/util/constants.h>
#include <openspace/util/factorymanager.h>

namespace {
	const std::string _loggerCat = "Scalegraph";
	const std::string KeyRadius = "Radius";
}

namespace openspace {
	
	Scalegraph* Scalegraph::createFromDictionary(const ghoul::Dictionary& dictionary) {
		if (!dictionary.hasValue<std::string>(KeyRadius)) {
			LERROR("Scalegraph did not have key '" << KeyRadius << "'");
			return nullptr;
		}

		std::string sceneRadius;
		dictionary.getValue(KeyRadius, sceneRadius);
		ghoul::TemplateFactory<Scalegraph>* factory
			= FactoryManager::ref().factory<Scalegraph>();
		Scalegraph* result = factory->create(sceneRadius, dictionary);
		if (result == nullptr) {
			LERROR("Failed creating ScaleGraph radius object'" << sceneRadius << "'");
			return nullptr;
		}

		return result;
	}

	Scalegraph::Scalegraph() {}

	Scalegraph::Scalegraph(const ghoul::Dictionary& dictionary) {}

	Scalegraph::~Scalegraph() {}

	bool Scalegraph::initialize() {
		return true;
	}

	void Scalegraph::update(const UpdateData& data) {}

} // namespace openspace