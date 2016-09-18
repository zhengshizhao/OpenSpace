return {
    -- Neptune barycenter module
    {
        Name = "NeptuneBarycenter",
        Parent = "SolarSystemBarycenter",
        -- SceneRadius unit is KM                
		SceneRadius = 1.0E+6,
        --Ephemeris = {
        --    Type = "Static"
        --}
        Ephemeris = {
            Type = "Spice",
            Body = "NEPTUNE BARYCENTER",
            Reference = "ECLIPJ2000",
            Observer = "SUN",
            Kernels = {
                "${OPENSPACE_DATA}/spice/de430_1850-2150.bsp"
            }
        },
    },

    -- Neptune module
    {   
        Name = "Neptune",
        Parent = "NeptuneBarycenter",
        -- SceneRadius unit is KM                
		SceneRadius = 3.0E+5,
        Renderable = {
            Type = "RenderablePlanet",
            Frame = "IAU_NEPTUNE",
            Body = "NEPTUNE BARYCENTER",
            Geometry = {
                Type = "SimpleSphere",
                Radius = { 2.4622 , 7 },
                Segments = 100
            },
            Textures = {
                Type = "simple",
                Color = "textures/neptune.jpg",
            },
        },
        --Ephemeris = {
        --    Type = "Spice",
        --    Body = "NEPTUNE BARYCENTER",
        --    Reference = "ECLIPJ2000",
        --    Observer = "SUN",
        --    Kernels = {
        --        "${OPENSPACE_DATA}/spice/de430_1850-2150.bsp"
        --    }
        --},
        Rotation = {
            Type = "Spice",
            Frame = "IAU_NEPTUNE",
            Reference = "ECLIPJ2000"
        },
        GuiName = "/Solar/Planets/Neptune"
    },
    -- NeptuneTrail module
    {   
        Name = "NeptuneTrail",
        --Parent = "NeptuneBarycenter",
        Parent = "SolarSystemBarycenter",
        Renderable = {
            Type = "RenderableTrail",
            Body = "NEPTUNE BARYCENTER",
            Frame = "GALACTIC",
            Observer = "SUN",
            RGB = {0.2, 0.5, 1.0 },
            TropicalOrbitPeriod = 59799.9 ,
            EarthOrbitRatio = 163.73,
            DayLength = 16.11,
            Textures = {
                Type = "simple",
                Color = "${COMMON_MODULE}/textures/glare_blue.png",
                -- need to add different texture
            },  
        },
        GuiName = "/Solar/NeptuneTrail"
    }
}
