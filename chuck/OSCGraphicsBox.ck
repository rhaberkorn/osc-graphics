public class OSCGraphicsBox extends OSCGraphicsLayer {
	class ColorPort extends OSCGraphicsPort {
		OSCGraphicsBox @layer;

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
}
