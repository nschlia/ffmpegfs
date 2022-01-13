BEGIN TRANSACTION;
CREATE TABLE `version` (
	`min_version_major`	INTEGER NOT NULL,
	`min_version_minor`	INTEGER NOT NULL
);
INSERT INTO `version` VALUES (1,97);
CREATE TABLE `cache_entry` (
    `filename`             TEXT NOT NULL,
    `desttype`             CHAR ( 10 ) NOT NULL,
    `enable_ismv`          BOOLEAN NOT NULL,
    `audiobitrate`         UNSIGNED INT NOT NULL,
    `audiosamplerate`      UNSIGNED INT NOT NULL,
    `videobitrate`         UNSIGNED INT NOT NULL,
    `videowidth`           UNSIGNED INT NOT NULL,
    `videoheight`          UNSIGNED INT NOT NULL,
    `deinterlace`          BOOLEAN NOT NULL,
    `duration`             UNSIGNED BIG INT NOT NULL,
    `predicted_filesize`   UNSIGNED BIG INT NOT NULL,
    `encoded_filesize`     UNSIGNED BIG INT NOT NULL,
    `video_frame_count`    UNSIGNED BIG INT NOT NULL,
    `segment_count`        UNSIGNED BIG INT NOT NULL,
    `finished`             INT NOT NULL,
    `error`                BOOLEAN NOT NULL,
    `errno`                INT NOT NULL,
    `averror`              INT NOT NULL,
    `creation_time`        DATETIME NOT NULL,
    `access_time`          DATETIME NOT NULL,
    `file_time`            DATETIME NOT NULL,
    `file_size`            UNSIGNED BIG INT NOT NULL,
    PRIMARY KEY(`filename`,`desttype`)
);
COMMIT;
