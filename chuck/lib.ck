"/usr/local" => string prefix;

prefix+"/share/osc-graphics/chuck" => string path;

path+"/OSCGraphicsLayer.ck" => Machine.add;
path+"/OSCGraphicsImage.ck" => Machine.add;
path+"/OSCGraphicsVideo.ck" => Machine.add;
path+"/OSCGraphicsBox.ck" => Machine.add;

path+"/OSCGraphics.ck" => Machine.add;
