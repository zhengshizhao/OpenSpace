return {
    {
        Name = "TLE-Test",
        Parent = "Earth",
        Renderable = {
            Type = "RenderablePlane",
            Size = {3.0, 4.0},
            Origin = "Center",
            Billboard = true,
            Texture = "tle.jpg"
        },
        Ephemeris = {
            Type = "TLE",
            File = "iss.tle"
        }
    }
}