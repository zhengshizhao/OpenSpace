return {
    -- Europa module
    {   
        Name = "Europa",
        Parent = "JupiterBarycenter",
        Renderable = {
            Type = "RenderablePlanetProjection",
            Frame = "IAU_EUROPA", 
            Body = "EUROPA",
            Geometry = {
                Type = "SimpleSphere",
                Radius = { 1.8213, 6 },
                Segments = 100
            },
            Textures = {
                Type = "simple",
                Color = "textures/europa.jpg",
                Project = "textures/defaultProj.png",
                Sequencing = "true",
            },
            Projection = {
                Observer   = "NEW HORIZONS",
                Target     = "EUROPA",
                Aberration = "NONE",
            },
            Instrument = {                
                Name       = "NH_LORRI",
                Method     = "ELLIPSOID",
                Aberration = "NONE",
                Fovy       = 0.2907,
                Aspect     = 1,
                Near       = 0.2,
                Far        = 10000,
            },
            PotentialTargets = {
                "JUPITER", "IO", "EUROPA", "GANYMEDE", "CALLISTO"
            }            
        },
        Ephemeris = {
            Type = "Spice",
            Body = "EUROPA",
            Reference = "ECLIPJ2000",
            Observer = "JUPITER BARYCENTER",
            Kernels = {
                "${OPENSPACE_DATA}/spice/de430_1850-2150.bsp"
            }
        },
        Rotation = {
            Type = "Spice",
            Frame = "IAU_EUROPA",
            Reference = "ECLIPJ2000"
        },
        GuiName = "/Solar/Planets/Jupiter"
    },
    {
        Name = "EuropaText",
        Parent = "Europa",
        Renderable = {
            Type = "RenderablePlane",
            Size = {1.0, 7.4},
            Origin = "Center",
            Billboard = true,
            Texture = "textures/Europa-Text.png"
        },
        Ephemeris = {
            Type = "Static",
            Position = {0, -1, 0, 7}
        }
    },    
    -- EuropaTrail module
    {   
        Name = "EuropaTrail",
        Parent = "JupiterBarycenter",
        Renderable = {
            Type = "RenderableTrail",
            Body = "EUROPA",
            Frame = "GALACTIC",
            Observer = "JUPITER BARYCENTER",
            RGB = { 0.7, 0.4, 0.2 },
            TropicalOrbitPeriod =  80 ,
            EarthOrbitRatio = 0.009,
            DayLength = 9.9259,
            LineFade = 2.0,
            Textures = {
                Type = "simple",
                Color = "${COMMON_MODULE}/textures/glare_blue.png",
                -- need to add different texture
            },  
        },
        GuiName = "/Solar/EuropaTrail"
    }
}
