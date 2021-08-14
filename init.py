import os
import shutil


zkPath = os.getenv('ZOOKEEPER_HOME')

zkdata_dir = zkPath + "/data"
zk_config = zkPath + "/conf/zoo.cfg"
zk_config_sample= zkPath + "/conf/zoo_sample.cfg"

if os.path.exists(zk_config):
    print "Exists zoo.cfg"
else :
    print "No zoo.cfg , cpoy that"
    shutil.copy(zk_config_sample, zk_config)
    f = open(zk_config,"r+")
    flist = f.readlines()
    flist[11] = "dataDir=" + zkdata_dir + "\n"
    f = open(zk_config,"w+")
    f.writelines(flist)
    f.close

