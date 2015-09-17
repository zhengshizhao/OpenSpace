__timeouts = {};
__nextTimeoutId = 0;

function setFrameTimeout(cb, frames)
    if (frames > -1) then
        __timeouts[__nextTimeoutId] = {frames, cb};
	__nextTimeoutId = __nextTimeoutId + 1;
	return __nextTimeoutId - 1;
    end
end

function clearFrameTimeout(id)
    __timeouts[id] = nil;
end


function __tickFrameTimeouts()
    for id, value in pairs(__timeouts) do
    	__timeouts[id][1] = __timeouts[id][1] - 1;
        if __timeouts[id][1] <= 0 then
            __timeouts[id][2]();
            __timeouts[id] = nil;
        end
    end
end

--set timeout for 10 frames
--handle = setFrameTimeout(function() openspace.aBufferToFile("screenshot0.tga") end, 10);
--clear timeout
--clearFrameTimeout(handle);
