if UseAccurateNewHorizonsKernels then
    NewHorizonsKernels = {
        "${SPICE}/nh_kernels/spk/NavSE_plu047_od122.bsp"
    }
else
    NewHorizonsKernels = {
        "${SPICE}/NewHorizonsKernels/nh_p4p5_revised.bsp"
    }
end

return {
    -- Nix module
    {   
        Name = "Nix",
        Parent = "PlutoBarycenter",
        Renderable = {
            Type = "RenderablePlanet",
            Frame = "IAU_PLUTO",
            Body = "NIX",
            Geometry = {
                Type = "SimpleSphere",
                Radius = { 0.45 , 5 },
                Segments = 100
            },
            Textures = {
                Type = "simple",
                Color = "textures/gray.jpg",
            }
        },
        Ephemeris = {
            Type = "Spice",
            Body = "NIX",
            Reference = "ECLIPJ2000",
            Observer = "PLUTO BARYCENTER",
            Kernels = NewHorizonsKernels
        },
        Rotation = {
            Type = "Spice",
            Frame = "IAU_PLUTO",
            Reference = "ECLIPJ2000"
        },
        GuiName = "/Solar/Planets/Nix"
    },
    {
        Name = "NixText",
        Parent = "Nix",
        Renderable = {
            Type = "RenderablePlane",
            Size = {1.0, 6.3},
            Origin = "Center",
            Billboard = true,
            Texture = "textures/Nix-Text.png"
        },
        Ephemeris = {
            Type = "Static",
            Position = {0, 0, 0, 0}
        }
    },
    -- StyxTrail module
    {   
        Name = "NixTrail",
        Parent = "PlutoBarycenter",
        Renderable = {
            Type = "RenderableTrail",
            Body = "NIX",
            Frame = "GALACTIC",
            Observer = "PLUTO BARYCENTER",
            RGB = {0.00,0.62,1.00},
            TropicalOrbitPeriod = 150 ,
            EarthOrbitRatio = 0.12,
            DayLength = 1,
            DayLength = 16.11,
            Textures = {
                Type = "simple",
                Color = "${COMMON_MODULE}/textures/glare_blue.png",
                -- need to add different texture
            },  
        },
        GuiName = "/Solar/NixTrail"
    }
    
}
