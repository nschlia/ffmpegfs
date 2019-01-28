sed '1,/OPTIONS/d;/General\/FUSE options/,$d' ../ffmpegfs.1.text > ffmpegfshelp
xxd -i ffmpegfshelp > $1
rm ffmpegfshelp


