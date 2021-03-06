return {
    -- Mars barycenter module
    {
        Name = "MarsBarycenter",
        Parent = "SolarSystemBarycenter",
        Ephemeris = {
            Type = "Static"
        }
    },

    -- Mars module
    {   
        Name = "Mars",
        Parent = "MarsBarycenter",
        Renderable = {
            Type = "RenderablePlanet",
            Frame = "IAU_MARS",
            Body = "MARS BARYCENTER",
            Geometry = {
                Type = "SimpleSphere",
                Radius = { 6.390, 6 },
                Segments = 100
            },
            Textures = {
                Type = "simple",
                Color = "textures/mars.jpg",
            },
        },
        Ephemeris = {
            Type = "Spice",
            Body = "MARS BARYCENTER",
            Reference = "ECLIPJ2000",
            Observer = "SUN",
            Kernels = {
                "${OPENSPACE_DATA}/spice/de430_1850-2150.bsp"
            }
        },
        Rotation = {
            Type = "Spice",
            Frame = "IAU_MARS",
            Reference = "ECLIPJ2000"
        },
        GuiName = "/Solar/Planets/Mars"
    },
    -- MarsTrail module
    {   
        Name = "MarsTrail",
        Parent = "MarsBarycenter",
        Renderable = {
            Type = "RenderableTrail",
            Body = "MARS BARYCENTER",
            Frame = "GALACTIC",
            Observer = "SUN",
            RGB = { 1, 0.8, 0.5 },
            TropicalOrbitPeriod = 686.973,
            EarthOrbitRatio = 1.881,
            DayLength = 24.6597,
            Textures = {
                Type = "simple",
                Color = "${COMMON_MODULE}/textures/glare_blue.png",
                -- need to add different texture
            },  
        },
        GuiName = "/Solar/MarsTrail"
    }
}
