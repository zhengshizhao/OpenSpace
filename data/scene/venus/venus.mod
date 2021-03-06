return {
    -- Venus barycenter module
    {
        Name = "VenusBarycenter",
        Parent = "SolarSystemBarycenter",
        Ephemeris = {
            Type = "Static"
        }
    },

    -- Venus module
    {   
        Name = "Venus",
        Parent = "VenusBarycenter",
        Renderable = {
            Type = "RenderablePlanet",
            Frame = "IAU_VENUS",
            Body = "VENUS",
            Geometry = {
                Type = "SimpleSphere",
                Radius = { 3.760, 6 },
                Segments = 100
            },
            Textures = {
                Type = "simple",
                Color = "textures/venus.jpg",
            },
        },
        Ephemeris = {
            Type = "Spice",
            Body = "VENUS",
            Reference = "ECLIPJ2000",
            Observer = "SUN",
            Kernels = {
                "${OPENSPACE_DATA}/spice/de430_1850-2150.bsp"
            }
        },
        Rotation = {
            Type = "Spice",
            Frame = "IAU_VENUS",
            Reference = "ECLIPJ2000"
        },
        GuiName = "/Solar/Planets/VENUS"
    },
    -- VenusTrail module
    {   
        Name = "VenusTrail",
        Parent = "VenusBarycenter",
        Renderable = {
            Type = "RenderableTrail",
            Body = "VENUS",
            Frame = "GALACTIC",
            Observer = "SUN",
            RGB = {1, 0.5, 0.2},
            TropicalOrbitPeriod = 224.695 ,
            EarthOrbitRatio = 0.615,
            DayLength = 2802.0,
            Textures = {
                Type = "simple",
                Color = "${COMMON_MODULE}/textures/glare_blue.png",
                -- need to add different texture
            },  
        },
        GuiName = "/Solar/VenusTrail"
    }
}
