return {
    {
        Name = "Spice-Reference",
        Parent = "Earth",
        Renderable = {
            Type = "RenderablePlane",
            Size = {3.0, 4.0},
            Origin = "Center",
            Billboard = true,
            Texture = "textures/spice.jpg"
        },
        Ephemeris = {
            Type = "Spice",
            Body = "-125544",
            Reference = "ECLIPJ2000",
            Observer = "EARTH",
            Kernels = {
                "iss.bsp",
                "${OPENSPACE_DATA}/spice/de430_1850-2150.bsp"
            }
        }
    }
}
