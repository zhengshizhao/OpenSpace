-- Change these settings to match the .mod-file
__manualSettings = {
    enlilModel = "nh_8_256_8",
    nTimesteps = 8,
    volumeRes = 256,
    brickSize = 8,
    transferFunction = "fire",
};

-- Simple pre-calculations
nBricksPerDim = __manualSettings.volumeRes / __manualSettings.brickSize;
nOtNodes = nBricksPerDim * nBricksPerDim * nBricksPerDim;
expToOtNodes = {};
exp = 0;
while true do
    if 2^exp > nOtNodes then break; end
    expToOtNodes[exp] = 2^exp;
    exp = exp + 1;
end

allTimeSteps = {};
for i = 0, __manualSettings.nTimesteps - 1 do
    allTimeSteps[i] = i;
end

----------------------------- Tests -----------------------------

-- Test different brick selectors under different budget settings
function testBrickSelectors()
    local commonSettings = {
        testName = "brickSelectors",
        stepSizeCoefficient = 1
    };

    local referenceSettings = {
        brickSelector = "tf",
        memoryBudget = nOtNodes,
        streamingBudget = nOtNodes
    };

    local variableSettings = {
        {
            property = "brickSelector",
            values = {"tf", "simple", "local"}
        }, {
            property = "view",
            values = {"regular", "top", "side", "inside"}
        }, {
            property = "memoryBudget",
            values = expToOtNodes
        }, {
            property = "streamingBudget",
            values = expToOtNodes
        }, {
            property = "timestep",
            values = allTimeSteps
        }
    };

    __runTest(commonSettings, referenceSettings, variableSettings);
end

-- Test different memory budgets while keeping other parameters constant
function testMemoryBudget()
    local commonSettings = {
        testName = "memoryBudget",
        brickSelector = "tf",
        timestep = 0,
        streamingBudget = nOtNodes,
    };

    local referenceSettings = {
        memoryBudget = nOtNodes,
        stepSizeCoefficient = 1
    };

    local variableSettings = {
        {
            property = "view",
            values = {"regular", "top", "side", "inside"}
        }, {
            property = "memoryBudget",
            values = expToOtNodes
        }, {
            property = "stepSizeCoefficient",
            values = {1, 10000} -- 10000 => "no rendering time"
        }, {
            property = "timestep",
            values = allTimeSteps
        }
    };

    __runTest(commonSettings, referenceSettings, variableSettings);
end

-- Test different streaming budgets while keeping other parameters constant
function testStreamingBudget()
    local commonSettings = {
        testName = "streamingBudget",
        brickSelector = "tf",
        timestep = 0,
        memoryBudget = nOtNodes,
    };

    local referenceSettings = {
        streamingBudget = nOtNodes,
        stepSizeCoefficient = 1
    };

    local variableSettings = {
        {
            property = "view",
            values = {"regular", "top", "side", "inside"}
        }, {
            property = "streamingBudget",
            values = expToOtNodes
        }, {
            property = "stepSizeCoefficient",
            values = {1, 10000} -- 10000 => "no rendering time"
        }, {
            property = "timestep",
            values = allTimeSteps
        }
    };

    __runTest(commonSettings, referenceSettings, variableSettings);
end

-- Test different step size coefficients while keeping other parameters constant
function testStepSize()
    local commonSettings = {
        testName = "stepSizeCoefficient",
        brickSelector = "tf",
        memoryBudget = nOtNodes,
        streamingBudget = nOtNodes,
    };

    local referenceSettings = {
        stepSizeCoefficient = 0.1
    };

    stepSizeCoefficients = {};
    for i = 1, 20 do
        stepSizeCoefficients[i - 1] = i / 2;
    end

    local variableSettings = {
        {
            property = "view",
            values = {"regular", "top", "side", "inside"}
        }, {
            property = "stepSizeCoefficient",
            values = stepSizeCoefficients
        }, {
            property = "timestep",
            values = allTimeSteps
        }
    };

    __runTest(commonSettings, referenceSettings, variableSettings);
end



------------------------- Test execution -------------------------

__benchmarkSettings = {};
__timeoutDelay = 0;
__busy = false;
__printingRefStats = false;
__printingRefImages = false;
__printingStats = false;
__printingImages = false;

-- Run test with given settings
function __runTest(commonSettings, referenceSettings, variableSettings)
    -- commonSettings: Common settings for all test cases
    -- referenceSettings: Settings to use when producing a reference image
    -- variableSettings: table of properties to vary and their values

    if __busy then
        openspace.printError("Cannot start running " .. commonSettings.testName .. " until " .. __benchmarkSettings.testName .. " has finished");
        return;
    end
    __busy = true;

    -- Make sure settings are reset
    __resetSettings(commonSettings);

    -- Validate reference settings and remove invalid properties
    __validateReferenceSettings(referenceSettings);

    -- Setup project
    __setTestProperties(commonSettings);
    __setTestProperties(referenceSettings);

    -- Reverse to get most significant property last
    local varSettings = __reverseTable(variableSettings);

    -- Check if there are any variable settings that requires separate reference images
    local variableReferenceSettings = {};
    local k = 1;
    local nTestCases = 0;
    for key, value in ipairs(varSettings) do
        if value.property == "view" or value.property == "timestep" then
            variableReferenceSettings[k] = value;
            k = k + 1;
        elseif referenceSettings[value.property] == nil and commonSettings[value.property] == nil then
            openspace.printError("Property " .. value.property .. " needs to be specified in referenceSettings or commonSettings");
            return;
        end
        if nTestCases == 0 then
            nTestCases = #value.values;
        else
            nTestCases = nTestCases * #value.values;
        end
    end

    -- Print some info about the test
    openspace.printInfo("Running test <" .. __benchmarkSettings.testName .. "> with " .. nTestCases .. " test cases");

    -- Create reference images and reference stats files
    __createReferences(variableReferenceSettings, false);
    __createReferences(variableReferenceSettings, true);

    -- Overwrite reference settings if another value is given in common settings
    __setTestProperties(commonSettings);

    __executeTests(varSettings, false);
    __executeTests(varSettings, true);

    -- Set __busy to false
    __release();
end


------------------------ Helper functions ------------------------

-- Reset settings
function __resetSettings(commonSettings)
    __setTestName("untitled");
    __setTimestep(0);
    __setMemoryBudget(0);
    __setStreamingBudget(0);
    __setStepSizeCoefficient(1);
    __timeoutDelay = 1;
    __printingRefStats = false;
    __printingRefImages = false;
    __printingStats = false;
    __printingImages = false;

    if commonSettings.view == nil then
        -- Only do this if it's not going to be set by common settings
        __setView("regular");
    end

    if __benchmarkSettings[brickSelector] == nil then
        -- Only do this if nothing is set
        __setBrickSelector("tf");
    end
end

-- Validate reference settings and remove invalid properties
function __validateReferenceSettings(referenceSettings)
    if referenceSettings.view ~= nil then
        openspace.printWarning("Reference settings cannot contain view. Specify in commonSettings or in variableSettings instead.");
        referenceSettings.view = nil;
    end
    if referenceSettings.timestep ~= nil then
        openspace.printWarning("Reference settings cannot contain timestep. Specify in commonSettings or in variableSettings instead.");
        referenceSettings.timestep = nil;
    end
end

-- Execute tests recursively with given settings
function __executeTests(variableSettings, saveImage)
    -- if saveImage, capture screenshot, otherwise save stats to txt

    local len = #variableSettings;
    if len > 0 then
        local remainingSettings = __copyTable(variableSettings);
        local property = remainingSettings[len];
        remainingSettings[len] = nil;
        local propertyName = property["property"];
        local propertyValues = property["values"];
        for i, value in ipairs(propertyValues) do
            function setProperty()
                __setTestProperty(propertyName, value);
            end
            setFrameTimeout(setProperty, __timeoutDelay);
            __executeTests(remainingSettings, saveImage, false);
        end
    else
        if saveImage then
            setFrameTimeout(__captureScreenshot, __timeoutDelay);
        else
            setFrameTimeout(__saveStatsToFile, __timeoutDelay);
        end
        __timeoutDelay = __timeoutDelay + 1;
    end
end

-- Create references for all combinations of variable settings
function __createReferences(variableSettings, saveImage)
    -- if saveImage, capture screenshot, otherwise save stats to txt

    local len = #variableSettings;
    if len > 0 then
        local remainingSettings = __copyTable(variableSettings);
        local property = remainingSettings[len];
        remainingSettings[len] = nil;
        local propertyName = property["property"];
        local propertyValues = property["values"];
        for i, value in ipairs(propertyValues) do
            function setProperty()
                __setTestProperty(propertyName, value);
            end
            setFrameTimeout(setProperty, __timeoutDelay);
            __createReferences(remainingSettings, saveImage, false);
        end
    else
        if saveImage then
            setFrameTimeout(__captureReference, __timeoutDelay);
        else
            setFrameTimeout(__saveReferenceStats, __timeoutDelay);
        end
        __timeoutDelay = __timeoutDelay + 1;
    end
end

-- Set __busy to false when the test is finished
function __release()
    function notBusy()
        __busy = false;
        openspace.printInfo("Test <" .. __benchmarkSettings.testName .. "> finished successfully");
    end
    setFrameTimeout(notBusy, __timeoutDelay);
end

-- Reverse a table with integer keys
function __reverseTable(itable)
    local reversedTable = {};
    local itemCount = #itable;
    for k, v in ipairs(itable) do
        if k > (itemCount+1)/2 then break; end

        local l = itemCount + 1 - k;
        reversedTable[k] = itable[l];
        reversedTable[l] = itable[k];
    end
    return reversedTable;
end

-- Perform deep copy on a table
function __copyTable(orig)
    local origType = type(orig);
    local copy;
    if origType == 'table' then
        copy = {};
        for k, v in next, orig, nil do
            copy[__copyTable(k)] = __copyTable(v);
        end
        setmetatable(copy, __copyTable(getmetatable(orig)))
    else -- number, string, boolean, etc
        copy = orig;
    end
    return copy
end

-- Set multiple settings
function __setTestProperties(properties)
    for propName, value in pairs(properties) do
        __setTestProperty(propName, value);
    end
end

-- Pass arguments to the right function depending on property name
function __setTestProperty(propertyName, value)
    if propertyName == "testName" then
        __setTestName(value);
    elseif propertyName == "view" then
        __setView(value);
    elseif propertyName == "brickSelector" then
        __setBrickSelector(value);
    elseif propertyName == "timestep" then
        __setTimestep(value);
    elseif propertyName == "memoryBudget" then
        __setMemoryBudget(value);
    elseif propertyName == "streamingBudget" then
        __setStreamingBudget(value);
    elseif propertyName == "stepSizeCoefficient" then
        __setStepSizeCoefficient(value);
    end
end

-- Set test name to use in output files
function __setTestName(testName)
    __benchmarkSettings.testName = testName;
end

-- Disable looping and set timestep to use in the volume
function __setTimestep(timestep)
    openspace.setPropertyValue("Enlil.renderable.loop", false);
    openspace.setPropertyValue("Enlil.renderable.useGlobalTime", false);
    openspace.setPropertyValue("Enlil.renderable.currentTime", timestep);
    __benchmarkSettings.timestep = timestep;
end

-- Set memory budget
function __setMemoryBudget(memoryBudget)
    openspace.setPropertyValue("Enlil.renderable.memoryBudget", memoryBudget);
    __benchmarkSettings.memoryBudget = memoryBudget;
end

-- Set streaming budget
function __setStreamingBudget(streamingBudget)
    openspace.setPropertyValue("Enlil.renderable.streamingBudget", streamingBudget);
    __benchmarkSettings.streamingBudget = streamingBudget;
end

-- Set which brickSelector to use {"tf", "simple", "local"}
function __setBrickSelector(brickSelector)
    openspace.setPropertyValue("Enlil.renderable.selector", brickSelector);
    __benchmarkSettings.brickSelector = brickSelector;
end

-- Set which view to use, out of a few predefined
function __setView(viewName)
    if __benchmarkSettings.view == viewName then
        return;
    end
    if viewName == "regular" then
        --openspace.camera.setPosition(-3.07853, 4.49254, 5.96032, 12);
        openspace.camera.setPosition(0, 4, 8, 12);
    elseif viewName == "side" then
        --openspace.camera.setPosition(0.105648, -0.576608, 1.1016, 13);
        openspace.camera.setPosition(0, -5, 10, 12);
    elseif viewName == "top" then
        --openspace.camera.setPosition(1.66395, 11.356, 7.50285, 12);
        openspace.camera.setPosition(0, 10, 10, 12);
    elseif viewName == "inside" then
        --openspace.camera.setPosition(-1.45315, -1.03335, 1.97789, 12);
        openspace.camera.setPosition(0, -4, 8, 11);
    else
        return;
    end
    __benchmarkSettings.view = viewName;
end

-- Set the coefficient to multiply the stepSize with
function __setStepSizeCoefficient(stepSizeCoefficient)
    openspace.setPropertyValue("Enlil.renderable.stepSizeCoefficient", stepSizeCoefficient);
    __benchmarkSettings.stepSizeCoefficient = stepSizeCoefficient;
end

-- Capture a screenshot and name the file "reference"
function __captureReference()
    __captureScreenshot(true);
end

-- Save stats for reference image
function __saveReferenceStats()
    __saveStatsToFile(true);
end

-- Create a file path from used settings, without extension
function __filePath(reference)
    local settings = __benchmarkSettings;
    local directory = "benchmarking/" .. settings.testName
                    .. "/" .. __manualSettings.enlilModel
                    .. "/" .. settings.view .. "View"
                    .. "/" .. __manualSettings.transferFunction .. "Tf"
                    .. "/timestep" .. settings.timestep;
    local filename = "";
    if (reference) then
        filename = "reference";
    else
        filename =  "bs-" .. settings.brickSelector
                .. "_mb-" .. settings.memoryBudget
                .. "_sb-" .. settings.streamingBudget
                .. "_sc-" .. settings.stepSizeCoefficient;
    end
    local filePath = directory .. "/" .. filename;
    return filePath;
end

-- Capture a screenshot and name the file properly
function __captureScreenshot(reference)
    if not reference and  not __printingImages then
        openspace.printInfo("Capturing screenshots for test <" .. __benchmarkSettings.testName .. ">");
        __printingImages = true;
    elseif reference and not __printingRefImages then
        openspace.printInfo("Capturing reference screenshots for test <" .. __benchmarkSettings.testName .. ">");
        __printingRefImages = true;
    end
    local filePath = __filePath(reference);
    local imagePath = filePath .. ".bmp";
    openspace.aBufferToFile(imagePath);
end

-- Save stats to file
function __saveStatsToFile(reference)
    if not reference and not __printingStats then
        openspace.printInfo("Gathering stats for test <" .. __benchmarkSettings.testName .. ">");
        __printingStats = true;
    elseif reference and not __printingRefStats then
        openspace.printInfo("Gathering reference stats for test <" .. __benchmarkSettings.testName .. ">");
        __printingRefStats = true;
    end
    local filePath = __filePath(reference);
    local statsPath = filePath .. ".txt";
    openspace.setPropertyValue("Enlil.renderable.printStatsFileName", statsPath);
    openspace.setPropertyValue("Enlil.renderable.printStats", true);
end