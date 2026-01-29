#version 330 core
        in vec2 TexCoords;
        out vec4 FragColor;

        uniform sampler2D text;
        uniform vec4 textColor;   // ← теперь vec4

        void main()
        {
            float glyphAlpha = texture(text, TexCoords).a;

            // итоговая альфа = глиф * visibility
            FragColor = vec4(
                textColor.rgb,
                glyphAlpha * textColor.a
            );
        }