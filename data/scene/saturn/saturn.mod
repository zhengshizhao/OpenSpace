return {
    -- Saturn barycenter module
    {
        Name = "SaturnBarycenter",
        Parent = "SolarSystemBarycenter",
        -- Scene Radius in KM:
        SceneRadius = 1.0E+6,
        --Ephemeris = {
        --    Type = "Static"
        --}
        Ephemeris = {
            Type = "Spice",
            Body = "SATURN BARYCENTER",
            Reference = "ECLIPJ2000",
            Observer = "SUN",
            Kernels = {
                "${OPENSPACE_DATA}/spice/de430_1850-2150.bsp"
            }
        },
    },

    -- Saturn module
    {   
        Name = "Saturn",
        Parent = "SaturnBarycenter",
        -- Scene Radius in KM:
        SceneRadius = 6.0E+5,
        Renderable = {
            Type = "RenderablePlanet",
            Frame = "IAU_SATURN",
            Body = "SATURN BARYCENTER",
            Geometry = {
                Type = "SimpleSphere",
                Radius = { 6.0268, 7 },
                Segments = 100
            },
            Textures = {
                Type = "simple",
                Color = "textures/saturn.jpg",
            },
        },
        --Ephemeris = {
        --    Type = "Spice",
        --    Body = "SATURN BARYCENTER",
        --    Reference = "ECLIPJ2000",
        --    Observer = "SUN",
        --    Kernels = {
        --        "${OPENSPACE_DATA}/spice/de430_1850-2150.bsp"
        --    }
        --},
        Rotation = {
            Type = "Spice",
            Frame = "IAU_SATURN",
            Reference = "ECLIPJ2000"
        },
        GuiName = "/Solar/Planets/Saturn"
    },
    -- The rings of Saturn
    -- Using the 'Saturn's rings dark side mosaic' as a basis
    {
        Name = "SaturnRings",
        Parent = "Saturn",
        Renderable = {
            Type = "RenderableRings",
            Body = "SATURN BARYCENTER",
            Frame = "IAU_SATURN",
            Texture = "textures/saturn_rings.png",
            Size = { 0.140445100671159, 9.0 }, -- 140445.100671159km
            Offset = { 74500 / 140445.100671159, 1.0 } -- min / max extend
        },

    },
    -- SaturnTrail module
    {   
        Name = "SaturnTrail",
        --Parent = "SaturnBarycenter",
        Parent = "SolarSystemBarycenter",
        Renderable = {
            Type = "RenderableTrail",
            Body = "SATURN BARYCENTER",
            Frame = "GALACTIC",
            Observer = "SUN",
            RGB = {0.85,0.75,0.51 },
            TropicalOrbitPeriod = 10746.94 ,
            EarthOrbitRatio = 29.424,
            DayLength = 10.656,
            Textures = {
                Type = "simple",
                Color = "${COMMON_MODULE}/textures/glare_blue.png",
                -- need to add different texture
            },  
        },
        GuiName = "/Solar/SaturnTrail"
    }
}
