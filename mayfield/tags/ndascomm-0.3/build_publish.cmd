@call cleanup_publish.cmd

@rem
@rem THIS LINES are for an workaround for removing racing making the directories from publish.js
@rem

@mkdir .\lib\fre\i386
@mkdir .\lib\fre\i386\kernel\i386
@mkdir .\lib\chk\i386
@mkdir .\lib\chk\i386\kernel\i386

@pushd .
@cd src
@call buildup.cmd
@cd ..
@popd
@pushd .
@cd publish
@call mkcat.cmd
@popd
