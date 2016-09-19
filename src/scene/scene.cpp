/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2016                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <openspace/scene/scene.h>

#include <openspace/engine/configurationmanager.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/interaction/interactionhandler.h>
#include <openspace/query/query.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scenegraphnode.h>
#include <openspace/scripting/scriptengine.h>
#include <openspace/scripting/script_helper.h>
#include <openspace/util/time.h>
#include <openspace/util/distancetoobject.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/misc/exception.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/lua/ghoul_lua.h>
#include <ghoul/lua/lua_helper.h>
#include <ghoul/misc/onscopeexit.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>

#include <iostream>
#include <iterator>
#include <fstream>
#include <string>
#include <chrono>

#ifdef OPENSPACE_MODULE_ONSCREENGUI_ENABLED
#include <modules/onscreengui/include/gui.h>
#endif

#include "scene_lua.inl"

namespace {
    const std::string _loggerCat = "Scene";
    const std::string _moduleExtension = ".mod";
    const std::string _defaultCommonDirectory = "common";
    const std::string _commonModuleToken = "${COMMON_MODULE}";
    const std::string _mostProbableSceneName = "SolarSystemBarycenter";
    const std::string _parentOfAllNodes = "Root";

    const std::string KeyCamera = "Camera";
    const std::string KeyFocusObject = "Focus";
    const std::string KeyPositionObject = "Position";
    const std::string KeyViewOffset = "Offset";
}

namespace openspace {

Scene::Scene() : _focus(SceneGraphNode::RootNodeName) {}

Scene::~Scene() {
    deinitialize();
}

bool Scene::initialize() {
    LDEBUG("Initializing SceneGraph");   
    return true;
}

bool Scene::deinitialize() {
    clearSceneGraph();
    return true;
}

void Scene::update(const UpdateData& data) {
    if (!_sceneGraphToLoad.empty()) {
        OsEng.renderEngine().scene()->clearSceneGraph();
        try {          
            loadSceneInternal(_sceneGraphToLoad);

            // Reset the InteractionManager to Orbital/default mode
            // TODO: Decide if it belongs in the scene and/or how it gets reloaded
            OsEng.interactionHandler().setInteractionMode("Orbital");

            // After loading the scene, the keyboard bindings have been set
            
            std::string type;
            std::string file;
            bool hasType = OsEng.configurationManager().getValue(
                ConfigurationManager::KeyKeyboardShortcutsType, type
            );
            
            bool hasFile = OsEng.configurationManager().getValue(
                ConfigurationManager::KeyKeyboardShortcutsFile, file
            );
            
            if (hasType && hasFile) {
                OsEng.interactionHandler().writeKeyboardDocumentation(type, file);
            }

            LINFO("Loaded " << _sceneGraphToLoad);
            _sceneGraphToLoad = "";
        }
        catch (const ghoul::RuntimeError& e) {
            LERROR(e.what());
            _sceneGraphToLoad = "";
            return;
        }
    }

    for (SceneGraphNode* node : _graph.nodes()) {
        try {
            node->update(data);
        }
        catch (const ghoul::RuntimeError& e) {
            LERRORC(e.component, e.what());
        }
    }
}

void Scene::evaluate(Camera* camera) {
    for (SceneGraphNode* node : _graph.nodes())
        node->evaluate(camera);
    //_root->evaluate(camera);
}

void Scene::render(const RenderData& data, RendererTasks& tasks) {
    for (SceneGraphNode* node : _graph.nodes()) {
        node->render(data, tasks);
    }
}

void Scene::postRender(const RenderData& data) {
    for (SceneGraphNode* node : _graph.nodes()) {
        node->postRender(data);
    }
}

void Scene::scheduleLoadSceneFile(const std::string& sceneDescriptionFilePath) {
    _sceneGraphToLoad = sceneDescriptionFilePath;
}

void Scene::clearSceneGraph() {
    LINFO("Clearing current scene graph");
    // deallocate the scene graph. Recursive deallocation will occur
    _graph.clear();
    //if (_root) {
    //    _root->deinitialize();
    //    delete _root;
    //    _root = nullptr;
    //}

 //   _nodes.erase(_nodes.begin(), _nodes.end());
 //   _allNodes.erase(_allNodes.begin(), _allNodes.end());

    _focus.clear();
}

bool Scene::loadSceneInternal(const std::string& sceneDescriptionFilePath) {
    ghoul::Dictionary dictionary;
    
    
    lua_State* state = ghoul::lua::createNewLuaState();
    OnExit(
           // Delete the Lua state at the end of the scope, no matter what
           [state](){ghoul::lua::destroyLuaState(state);}
           );
    
    OsEng.scriptEngine().initializeLuaState(state);

    ghoul::lua::loadDictionaryFromFile(
        sceneDescriptionFilePath,
        dictionary,
        state
    );

    _graph.loadFromFile(sceneDescriptionFilePath);

    // Initialize all nodes
    for (SceneGraphNode* node : _graph.nodes()) {
        try {
            bool success = node->initialize();
            if (success)
                LDEBUG(node->name() << " initialized successfully!");
            else
                LWARNING(node->name() << " not initialized.");
        }
        catch (const ghoul::RuntimeError& e) {
            LERRORC(_loggerCat + "(" + e.component + ")", e.what());
        }
    }

    // update the position of all nodes
    // TODO need to check this; unnecessary? (ab)
    for (SceneGraphNode* node : _graph.nodes()) {
        try {
            node->update({
                glm::dvec3(0),
                glm::dmat3(1),
                1,
                Time::ref().currentTime() });
        }
        catch (const ghoul::RuntimeError& e) {
            LERRORC(e.component, e.message);
        }
    }

    for (auto it = _graph.nodes().rbegin(); it != _graph.nodes().rend(); ++it)
        (*it)->calculateBoundingSphere();

    // Read the camera dictionary and set the camera state
    ghoul::Dictionary cameraDictionary;
    if (dictionary.getValue(KeyCamera, cameraDictionary)) {
        OsEng.interactionHandler().setCameraStateFromDictionary(cameraDictionary);
    }

    // If a PropertyDocumentationFile was specified, generate it now
    const bool hasType = OsEng.configurationManager().hasKey(ConfigurationManager::KeyPropertyDocumentationType);
    const bool hasFile = OsEng.configurationManager().hasKey(ConfigurationManager::KeyPropertyDocumentationFile);
    if (hasType && hasFile) {
        std::string propertyDocumentationType;
        OsEng.configurationManager().getValue(ConfigurationManager::KeyPropertyDocumentationType, propertyDocumentationType);
        std::string propertyDocumentationFile;
        OsEng.configurationManager().getValue(ConfigurationManager::KeyPropertyDocumentationFile, propertyDocumentationFile);

        propertyDocumentationFile = absPath(propertyDocumentationFile);
        writePropertyDocumentation(propertyDocumentationFile, propertyDocumentationType);
    }


    OsEng.runPostInitializationScripts(sceneDescriptionFilePath);

    OsEng.enableBarrier();

    return true;
}

//void Scene::loadModules(
//    const std::string& directory, 
//    const ghoul::Dictionary& dictionary) 
//{
//    // Struct containing dependencies and nodes
//    LoadMaps m;
//
//    // Get the common directory
//    std::string commonDirectory(_defaultCommonDirectory);
//    dictionary.getValue(constants::scenegraph::keyCommonFolder, commonDirectory);
//    FileSys.registerPathToken(_commonModuleToken, commonDirectory);
//
//    lua_State* state = ghoul::lua::createNewLuaState();
//    OsEng.scriptEngine()->initializeLuaState(state);
//
//    LDEBUG("Loading common module folder '" << commonDirectory << "'");
//    // Load common modules into LoadMaps struct
//    loadModule(m, FileSys.pathByAppendingComponent(directory, commonDirectory), state);
//
//    // Load the rest of the modules into LoadMaps struct
//    ghoul::Dictionary moduleDictionary;
//    if (dictionary.getValue(constants::scenegraph::keyModules, moduleDictionary)) {
//        std::vector<std::string> keys = moduleDictionary.keys();
//        std::sort(keys.begin(), keys.end());
//        for (const std::string& key : keys) {
//            std::string moduleFolder;
//            if (moduleDictionary.getValue(key, moduleFolder)) {
//                loadModule(m, FileSys.pathByAppendingComponent(directory, moduleFolder), state);
//            }
//        }
//    }
//
//    // Load and construct scenegraphnodes from LoadMaps struct
//    loadNodes(SceneGraphNode::RootNodeName, m);
//
//    // Remove loaded nodes from dependency list
//    for(const auto& name: m.loadedNodes) {
//        m.dependencies.erase(name);
//    }
//
//    // Check to see what dependencies are not resolved.
//    for(auto& node: m.dependencies) {
//        LWARNING(
//            "'" << node.second << "'' not loaded, parent '" 
//            << node.first << "' not defined!");
//    }
//}

//void Scene::loadModule(LoadMaps& m,const std::string& modulePath, lua_State* state) {
//    auto pos = modulePath.find_last_of(ghoul::filesystem::FileSystem::PathSeparator);
//    if (pos == modulePath.npos) {
//        LERROR("Bad format for module path: " << modulePath);
//        return;
//    }
//
//    std::string fullModule = modulePath + modulePath.substr(pos) + _moduleExtension;
//    LDEBUG("Loading nodes from: " << fullModule);
//
//    ghoul::filesystem::Directory oldDirectory = FileSys.currentDirectory();
//    FileSys.setCurrentDirectory(modulePath);
//
//    ghoul::Dictionary moduleDictionary;
//    ghoul::lua::loadDictionaryFromFile(fullModule, moduleDictionary, state);
//    std::vector<std::string> keys = moduleDictionary.keys();
//    for (const std::string& key : keys) {
//        if (!moduleDictionary.hasValue<ghoul::Dictionary>(key)) {
//            LERROR("SceneGraphElement '" << key << "' is not a table in module '"
//                                         << fullModule << "'");
//            continue;
//        }
//        
//        ghoul::Dictionary element;
//        std::string nodeName;
//        std::string parentName;
//
//        moduleDictionary.getValue(key, element);
//        element.setValue(constants::scenegraph::keyPathModule, modulePath);
//
//        element.getValue(constants::scenegraphnode::keyName, nodeName);
//        element.getValue(constants::scenegraphnode::keyParentName, parentName);
//
//        m.nodes[nodeName] = element;
//        m.dependencies.emplace(parentName,nodeName);
//    }
//
//    FileSys.setCurrentDirectory(oldDirectory);
//}

//void Scene::loadNodes(const std::string& parentName, LoadMaps& m) {
//    auto eqRange = m.dependencies.equal_range(parentName);
//    for (auto it = eqRange.first; it != eqRange.second; ++it) {
//        auto node = m.nodes.find((*it).second);
//        loadNode(node->second);
//        loadNodes((*it).second, m);
//    }
//    m.loadedNodes.emplace_back(parentName);
//}
//
//void Scene::loadNode(const ghoul::Dictionary& dictionary) {
//    SceneGraphNode* node = SceneGraphNode::createFromDictionary(dictionary);
//    if(node) {
//        _allNodes.emplace(node->name(), node);
//        _nodes.push_back(node);
//    }
//}

//void SceneGraph::loadModule(const std::string& modulePath) {
//    auto pos = modulePath.find_last_of(ghoul::filesystem::FileSystem::PathSeparator);
//    if (pos == modulePath.npos) {
//        LERROR("Bad format for module path: " << modulePath);
//        return;
//    }
//
//    std::string fullModule = modulePath + modulePath.substr(pos) + _moduleExtension;
//    LDEBUG("Loading modules from: " << fullModule);
//
//    ghoul::filesystem::Directory oldDirectory = FileSys.currentDirectory();
//    FileSys.setCurrentDirectory(modulePath);
//
//    ghoul::Dictionary moduleDictionary;
//    ghoul::lua::loadDictionaryFromFile(fullModule, moduleDictionary);
//    std::vector<std::string> keys = moduleDictionary.keys();
//    for (const std::string& key : keys) {
//        if (!moduleDictionary.hasValue<ghoul::Dictionary>(key)) {
//            LERROR("SceneGraphElement '" << key << "' is not a table in module '"
//                                         << fullModule << "'");
//            continue;
//        }
//        
//        ghoul::Dictionary element;
//        moduleDictionary.getValue(key, element);
//
//        element.setValue(constants::scenegraph::keyPathModule, modulePath);
//
//        //each element in this new dictionary becomes a scenegraph node. 
//        SceneGraphNode* node = SceneGraphNode::createFromDictionary(element);
//
//        _allNodes.emplace(node->name(), node);
//        _nodes.push_back(node);
//    }
//
//    FileSys.setCurrentDirectory(oldDirectory);
//
//    // Print the tree
//    //printTree(_root);
//}

SceneGraphNode* Scene::root() const {
    return _graph.rootNode();
}
    
SceneGraphNode* Scene::sceneGraphNode(const std::string& name) const {
    return _graph.sceneGraphNode(name);
}

std::vector<SceneGraphNode*> Scene::allSceneGraphNodes() const {
    return _graph.nodes();
}

SceneGraph& Scene::sceneGraph() {
    return _graph;
}

std::string Scene::currentSceneName(Camera* camera, std::string _nameOfScene) const {
    if (camera == nullptr || _nameOfScene.empty()) {
        LERROR("Camera object not allocated or empty name scene passed to the method.");
        return _mostProbableSceneName; // This choice is controversal. Better to avoid a crash.
    }
    const SceneGraphNode* node = sceneGraphNode(_nameOfScene);

    if (node == nullptr) {
        std::stringstream ss;
        ss << "There is no scenegraph node with name: " << _nameOfScene;
        LERROR(ss.str());
    }

    //Starts in the last scene we kow we were in, checks if we are still inside, if not check parent, continue until we are inside a scene
    //double _distance = (DistanceToObject::ref().distanceCalc(camera->position(), node->worldPosition()));
    double _distance = (DistanceToObject::ref().distanceCalc(camera->positionVec3(), node->worldPosition()));

    std::vector<SceneGraphNode*> childrenScene(node->children());
    size_t nrOfChildren = childrenScene.size();

    // Traverses the scenetree to find a scene we are within. 
    while (_distance > node->sceneRadius()) {
        if (node->parent() != nullptr) {
            node          = node->parent();
            _nameOfScene  = node->name();
            childrenScene = node->children();
            nrOfChildren  = childrenScene.size();
            _distance     = (DistanceToObject::ref().distanceCalc(camera->positionVec3(), node->worldPosition()));
            break;
        } else if (node->parent() == nullptr) {
            // We have reached the root. 
            //node = scene->allSceneGraphNodes()[0];
            _nameOfScene  = node->name();
            childrenScene = node->children();
            nrOfChildren  = childrenScene.size();
            break;
        }
    }

    //Check if we are inside a child scene of the current scene. 
    bool outsideAllChildScenes = false;

    while (!childrenScene.empty() && !outsideAllChildScenes) {
        for (size_t i = 0; i < nrOfChildren; ++i) {
            SceneGraphNode * childNode = childrenScene.at(i);
            //double _childDistance = DistanceToObject::ref().distanceCalc(camera->position(), childNode->worldPosition());
            double _childDistance = DistanceToObject::ref().distanceCalc(camera->position(), childNode->dynamicWorldPosition(*camera, childNode, this));            
            double childSceneRadius = childNode->sceneRadius();

            // Set the new scene that we are inside the scene radius.
            if (_childDistance < childSceneRadius) {
                node          = childNode;
                childrenScene = node->children();
                _nameOfScene  = node->name();
                nrOfChildren  = childrenScene.size();
                break;
            }

            if (nrOfChildren - 1 == i) {
                outsideAllChildScenes = true;
            }
        }
    }

    return _nameOfScene;
}
const glm::dvec3 Scene::currentDisplacementPosition(const std::string & cameraParent,
    const SceneGraphNode* target) const {
    if (target != nullptr) {

        std::vector<SceneGraphNode*> cameraPath;
        std::vector<SceneGraphNode*> targetPath;

        SceneGraphNode* cameraParentNode = sceneGraphNode(cameraParent);
        SceneGraphNode* commonParentNode;
        std::vector<SceneGraphNode*> commonParentPath;

        //Find common parent for camera and object
        std::string commonParentName(cameraParent);  // initiates to camera parent in case 
                                                     // other path is not found
        cameraPath = pathTo(cameraParentNode);
        targetPath = pathTo(sceneGraphNode(target->name()));

        commonParentNode = findCommonParentNode(cameraParent, target->name());
        commonParentName = commonParentNode->name();
        commonParentPath = pathTo(commonParentNode);

        //Find the path from the camera to the common parent

        glm::dvec3 collectorCamera(pathCollector(cameraPath, commonParentName, true));
        glm::dvec3 collectorTarget(pathCollector(targetPath, commonParentName, false));

        return collectorTarget + collectorCamera;
    }
    else {
        LERROR("Target scenegraph node is null.");
        return glm::dvec3(0.0, 0.0, 0.0);
    }
}

SceneGraphNode* Scene::findCommonParentNode(const std::string & firstPath, const std::string & secondPath) const {
    if (firstPath.empty() || secondPath.empty()) {
        LERROR("Empty scenegraph node name passed to the method.");
        return sceneGraphNode(_parentOfAllNodes); // This choice is controversal. Better to avoid a crash.
    }
    std::vector<SceneGraphNode*> firstPathNode  = pathTo(sceneGraphNode(firstPath));
    std::vector<SceneGraphNode*> secondPathNode = pathTo(sceneGraphNode(secondPath));
    std::string strCommonParent = commonParent(firstPathNode, secondPathNode);

    return sceneGraphNode(strCommonParent);
}

std::vector<SceneGraphNode*> Scene::pathTo(SceneGraphNode* node) const {
    std::vector<SceneGraphNode*> path;

    if (node == nullptr) {
        LERROR("Invalid (null) scenegraph node name passed to pathTo() method.");
        return path;
    }
        
    while (node->parent() != nullptr) {
        path.push_back(node);
        node = node->parent();
    }
    path.push_back(node);

    return path;
}

std::string Scene::commonParent(std::vector<SceneGraphNode*> t1, std::vector<SceneGraphNode*> t2) const {
    if (t1.empty() && t2.empty()) {
        LERROR("Empty paths passed to commonParent method.");
        return _parentOfAllNodes;
    }

    std::string commonParentReturn(_mostProbableSceneName);
    int iterator = 0;
    int min = std::min(t1.size(), t2.size());
    while (iterator < min && t1.back()->name() == t2.back()->name()) {
        commonParentReturn = t1.back()->name();
        t1.pop_back();
        t2.pop_back();
        iterator++;
    }
    
    return commonParentReturn;
}

glm::dvec3 Scene::pathCollector(const std::vector<SceneGraphNode*> & path, const std::string & commonParentName, 
    const bool inverse) const {
    if (path.empty() || commonParentName.empty()) {
        LERROR("Empty path or common parent name passed to pathCollector method.");
        return glm::dvec3();
    }

    SceneGraphNode* firstElement = path.front();
    glm::dvec3 collector(path.back()->position());

    int depth = 0;
    // adds all elements to the collector, continues untill commomParent is found.
    while (firstElement->name() != commonParentName) {
        if (inverse)
            collector = collector - firstElement->position();
        else
            collector = collector + firstElement->position();
        
        firstElement = path[++depth];
    }

    return collector;
}

void Scene::setRelativeOrigin(Camera* camera) const {
    if (camera == nullptr) {
        LERROR("Camera object not allocated. Relative origin not set.");
        return;
    }
    std::string target                            = camera->getParent();
    SceneGraphNode* commonParentNode              = sceneGraphNode(target);
    std::vector<SceneGraphNode*> commonParentPath = pathTo(commonParentNode);

    newCameraOrigin(commonParentPath, camera);
}

void Scene::newCameraOrigin(const std::vector<SceneGraphNode*> & commonParentPath, Camera* camera) const {
    if (commonParentPath.empty() || camera == nullptr) {
        LERROR("Empty common parent path or not allocated camera passed to newCameraOrigin method.");
        return;
    }
    glm::dvec3 displacementVector = camera->positionVec3();
    if (commonParentPath.size() > 1) { // <1 if in root system. 
        glm::dvec3 tempDvector;
        for (int i = commonParentPath.size() - 1; i >= 1; i--) {
            tempDvector        = commonParentPath[i - 1]->worldPosition() - commonParentPath[i]->worldPosition();
            displacementVector = displacementVector - tempDvector;
        }
    }

    //Move the camera to the position of the common parent (The new origin)
    //Then add the distance from the displacementVector to get the camera into correct position
    glm::dvec3 origin = commonParentPath[0]->position();    
    camera->setDisplacementVector(displacementVector);
    glm::dvec3 newOrigin(origin + displacementVector);
    camera->setPositionVec3(newOrigin);
}

void Scene::writePropertyDocumentation(const std::string& filename, const std::string& type) {
    if (type == "text") {
        LDEBUG("Writing documentation for properties");
        std::ofstream file(filename);
        if (!file.good()) {
            LERROR("Could not open file '" << filename << "' for writing property documentation");
            return;
        }

        using properties::Property;
        for (SceneGraphNode* node : _graph.nodes()) {
            std::vector<Property*> properties = node->propertiesRecursive();
            if (!properties.empty()) {
                file << node->name() << std::endl;

                for (Property* p : properties) {
                    file << p->fullyQualifiedIdentifier() << ":   " << p->guiName() << std::endl;
                }

                file << std::endl;
            }
        }
    }
    else
        LERROR("Undefined type '" << type << "' for Property documentation");
}

scripting::LuaLibrary Scene::luaLibrary() {
    return {
        "",
        {
            {
                "setPropertyValue",
                &luascriptfunctions::property_setValue,
                "string, *",
                "Sets all properties identified by the URI (with potential wildcards) in "
                "the first argument. The second argument can be any type, but it has to "
                "match the type that the property (or properties) expect."
            },
            {
                "setPropertyValueRegex",
                &luascriptfunctions::property_setValueRegex,
                "Sets all properties that pass the regular expression in the first "
                "argument. The second argument can be any type, but it has to match the "
                "type of the properties that matched the regular expression. The regular "
                "expression has to be of the ECMAScript grammar."
            },
            {
                "setPropertyValueSingle",
                &luascriptfunctions::property_setValueSingle,
                "string, *",
                "Sets a property identified by the URI in "
                "the first argument. The second argument can be any type, but it has to "
                "match the type that the property expects.",
                true
            },
            {
                "getPropertyValue",
                &luascriptfunctions::property_getValue,
                "string",
                "Returns the value the property, identified by "
                "the provided URI."
            },
            {
                "loadScene",
                &luascriptfunctions::loadScene,
                "string",
                "Loads the scene found at the file passed as an "
                "argument. If a scene is already loaded, it is unloaded first"
            },
            {
                "addSceneGraphNode",
                &luascriptfunctions::addSceneGraphNode,
                "table",
                "Loads the SceneGraphNode described in the table and adds it to the "
                "SceneGraph"
            },
            {
                "removeSceneGraphNode",
                &luascriptfunctions::removeSceneGraphNode,
                "string",
                "Removes the SceneGraphNode identified by name"
            }
        }
    };
}

}  // namespace openspace
