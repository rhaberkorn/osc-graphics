public class OSCGraphicsText extends OSCGraphicsLayer {
	class ColorPort extends OSCGraphicsPort {
		OSCGraphicsText @layer;

		int color[];
		int index;

		fun void
		tick(float in, float prev)
		{
			in $ int => color[index];

			/* optimize: avoid sending unnecessary messages */
			if (color[index] != (prev $ int))
				color => layer.color;
		}
	}
	fun OSCGraphicsPort @
	getColorPort(int color[], int index)
	{
		ColorPort p;
		this @=> p.layer;
		color @=> p.color;
		index => p.index;

		return p;
	}
	fun int[]
	color(int color[])
	{
		osc_send.startMsg("/layer/"+name+"/color", "iii");
		for (0 => int i; i < 3; i++)
			color[i] => osc_send.addInt;

		return color;
	}

	fun string
	text(string text)
	{
		osc_send.startMsg("/layer/"+name+"/text", "s");
		text => osc_send.addString;

		return text;
	}

	static int STYLE_BOLD;
	static int STYLE_ITALIC;
	static int STYLE_UNDERLINE;

	fun int
	style(int flags)
	{
		string style;

		if (flags & STYLE_BOLD)
			style +=> "b";
		if (flags & STYLE_ITALIC)
			style +=> "i";
		if (flags & STYLE_UNDERLINE)
			style +=> "u";

		osc_send.startMsg("/layer/"+name+"/style", "s");
		style => osc_send.addString;

		return flags;
	}
}
/* static initialization */
(1 << 1) => OSCGraphicsText.STYLE_BOLD;
(1 << 2) => OSCGraphicsText.STYLE_ITALIC;
(1 << 3) => OSCGraphicsText.STYLE_UNDERLINE;
