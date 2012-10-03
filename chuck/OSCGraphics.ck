public class OSCGraphics {
	static OscSend @osc_send;
	static int free_id;

	fun static void
	clear()
	{
		osc_send.startMsg("/layer/*/delete", "");
	}

	fun static string
	record(string file)
	{
		osc_send.startMsg("/recorder/start", "s");
		file => osc_send.addString;

		return file;
	}
	fun static void
	stopRecord()
	{
		osc_send.startMsg("/recorder/stop", "");
	}

	fun static OSCGraphicsImage @
	getImage(string name)
	{
		OSCGraphicsImage img;
		osc_send @=> img.osc_send;
		name => img.name;
		return img;
	}
	fun static OSCGraphicsImage @
	newImage(int pos, int geo[], float opacity, string file)
	{
		OSCGraphicsImage img;

		img.init(osc_send, "image", "s",
			 pos, "__image_"+free_id, geo, opacity);
		file => osc_send.addString;

		free_id++;
		return img;
	}
	fun static OSCGraphicsImage @
	newImage(int pos, int geo[], float opacity)
	{
		return newImage(pos, geo, opacity, "");
	}
	fun static OSCGraphicsImage @
	newImage(int pos, int geo[])
	{
		return newImage(pos, geo, 1., "");
	}
	fun static OSCGraphicsImage @
	newImage(int pos)
	{
		return newImage(pos, null, 1., "");
	}
	fun static OSCGraphicsImage @
	newImage()
	{
		return newImage(-1, null, 1., "");
	}

	fun static OSCGraphicsVideo @
	getVideo(string name)
	{
		OSCGraphicsVideo video;
		osc_send @=> video.osc_send;
		name => video.name;
		return video;
	}
	fun static OSCGraphicsVideo @
	newVideo(int pos, int geo[], float opacity, string url)
	{
		OSCGraphicsVideo video;

		video.init(osc_send, "video", "s",
			   pos, "__video_"+free_id, geo, opacity);
		url => osc_send.addString;

		free_id++;
		return video;
	}
	fun static OSCGraphicsVideo @
	newVideo(int pos, int geo[], float opacity)
	{
		return newVideo(pos, geo, 1., "");
	}
	fun static OSCGraphicsVideo @
	newVideo(int pos, int geo[])
	{
		return newVideo(pos, geo, 1., "");
	}
	fun static OSCGraphicsVideo @
	newVideo(int pos)
	{
		return newVideo(pos, null, 1., "");
	}
	fun static OSCGraphicsVideo @
	newVideo()
	{
		return newVideo(-1, null, 1., "");
	}

	fun static OSCGraphicsBox @
	getBox(string name)
	{
		OSCGraphicsBox box;
		osc_send @=> box.osc_send;
		name => box.name;
		return box;
	}
	fun static OSCGraphicsBox @
	newBox(int pos, int geo[], float opacity, int color[])
	{
		OSCGraphicsBox box;

		box.init(osc_send, "box", "iii",
			 pos, "__box_"+free_id, geo, opacity);
		for (0 => int i; i < 3; i++)
			color[i] => osc_send.addInt;

		free_id++;
		return box;
	}
	fun static OSCGraphicsBox @
	newBox(int pos, int geo[], int color[])
	{
		return newBox(pos, geo, 1., color);
	}
	fun static OSCGraphicsBox @
	newBox(int pos, int color[])
	{
		return newBox(pos, null, 1., color);
	}
	fun static OSCGraphicsBox @
	newBox(int color[])
	{
		return newBox(-1, null, 1., color);
	}

	fun static OSCGraphicsText @
	getText(string name)
	{
		OSCGraphicsText text;
		osc_send @=> text.osc_send;
		name => text.name;
		return text;
	}
	fun static OSCGraphicsText @
	newText(int pos, int geo[], float opacity, int color[],
		string txt, string font)
	{
		OSCGraphicsText text;

		text.init(osc_send, "text", "iiiss",
			  pos, "__text_"+free_id, geo, opacity);
		for (0 => int i; i < 3; i++)
			color[i] => osc_send.addInt;
		txt => osc_send.addString;
		font => osc_send.addString;

		free_id++;
		return text;
	}
	fun static OSCGraphicsText @
	newText(int pos, int geo[], int color[], string txt, string font)
	{
		return newText(pos, geo, 1., color, txt, font);
	}
	fun static OSCGraphicsText @
	newText(int geo[], int color[], string txt, string font)
	{
		return newText(-1, geo, 1., color, txt, font);
	}
}
/* static initialization */
new OscSend @=> OSCGraphics.osc_send;
OSCGraphics.osc_send.setHost("localhost", 7770);

1 => OSCGraphics.free_id;
