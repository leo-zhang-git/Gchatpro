/*
 Navicat Premium Data Transfer

 Source Server         : cntos7
 Source Server Type    : MySQL
 Source Server Version : 50736
 Source Host           : localhost:3306
 Source Schema         : Gchatpro

 Target Server Type    : MySQL
 Target Server Version : 50736
 File Encoding         : 65001

 Date: 23/04/2022 08:35:12
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for roomlog1
-- ----------------------------
DROP TABLE IF EXISTS `roomlog1`;
CREATE TABLE `roomlog1`  (
  `sendtime` timestamp(0) NOT NULL DEFAULT CURRENT_TIMESTAMP(0) ON UPDATE CURRENT_TIMESTAMP(0),
  `sender` int(11) NOT NULL,
  `text` longtext CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL,
  PRIMARY KEY (`sendtime`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Records of roomlog1
-- ----------------------------

-- ----------------------------
-- Table structure for roomlog2
-- ----------------------------
DROP TABLE IF EXISTS `roomlog2`;
CREATE TABLE `roomlog2`  (
  `sendtime` timestamp(0) NOT NULL DEFAULT CURRENT_TIMESTAMP(0) ON UPDATE CURRENT_TIMESTAMP(0),
  `sender` int(11) NOT NULL,
  `text` longtext CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL,
  PRIMARY KEY (`sendtime`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Records of roomlog2
-- ----------------------------

-- ----------------------------
-- Table structure for roomlog3
-- ----------------------------
DROP TABLE IF EXISTS `roomlog3`;
CREATE TABLE `roomlog3`  (
  `sendtime` timestamp(0) NOT NULL DEFAULT CURRENT_TIMESTAMP(0) ON UPDATE CURRENT_TIMESTAMP(0),
  `sender` int(11) NOT NULL,
  `text` longtext CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL,
  PRIMARY KEY (`sendtime`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Records of roomlog3
-- ----------------------------

-- ----------------------------
-- Table structure for roomlog_origin
-- ----------------------------
DROP TABLE IF EXISTS `roomlog_origin`;
CREATE TABLE `roomlog_origin`  (
  `sendtime` timestamp(0) NOT NULL DEFAULT CURRENT_TIMESTAMP(0) ON UPDATE CURRENT_TIMESTAMP(0),
  `sender` int(11) NOT NULL,
  `text` longtext CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL,
  PRIMARY KEY (`sendtime`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Records of roomlog_origin
-- ----------------------------

-- ----------------------------
-- Table structure for tRoom
-- ----------------------------
DROP TABLE IF EXISTS `tRoom`;
CREATE TABLE `tRoom`  (
  `id` int(11) NOT NULL,
  `name` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL,
  `members` longtext CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL,
  `create_time` timestamp(0) NOT NULL DEFAULT CURRENT_TIMESTAMP(0),
  PRIMARY KEY (`id`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Records of tRoom
-- ----------------------------
INSERT INTO `tRoom` VALUES (1, '群1', '[1,2,3]', '2022-04-22 15:39:19');
INSERT INTO `tRoom` VALUES (2, '群2', '[1,2]', '2022-04-22 15:39:31');
INSERT INTO `tRoom` VALUES (3, '群3', '[2]', '2022-04-22 15:39:44');

-- ----------------------------
-- Table structure for tUser
-- ----------------------------
DROP TABLE IF EXISTS `tUser`;
CREATE TABLE `tUser`  (
  `id` int(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `uaccount` varchar(25) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL,
  `upassword` varchar(25) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL,
  `uname` varchar(90) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL,
  `email` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `friList` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL,
  `roomList` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL,
  `friReq` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL,
  `roomReq` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL,
  `offline_message` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL,
  `creat_time` timestamp(0) NOT NULL DEFAULT CURRENT_TIMESTAMP(0),
  `lastlogin_time` timestamp(0) NOT NULL DEFAULT CURRENT_TIMESTAMP(0),
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE INDEX `uaccount`(`uaccount`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 4 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Records of tUser
-- ----------------------------
INSERT INTO `tUser` VALUES (1, 'root', 'root', 'root', NULL, '[2, 3]', '[1,2]', NULL, NULL, NULL, '2022-04-22 15:37:13', '2022-04-22 15:37:13');
INSERT INTO `tUser` VALUES (2, 'a', '123', 'a', NULL, '[1,3]', '[1,2,3]', NULL, NULL, NULL, '2022-04-22 15:37:34', '2022-04-23 08:26:22');
INSERT INTO `tUser` VALUES (3, 'b', '123', 'b', NULL, '[1,2]', '[1]', NULL, NULL, NULL, '2022-04-22 15:37:46', '2022-04-22 15:37:46');

-- ----------------------------
-- Procedure structure for del_oldroomlog
-- ----------------------------
DROP PROCEDURE IF EXISTS `del_oldroomlog`;
delimiter ;;
CREATE PROCEDURE `del_oldroomlog`()
BEGIN
	#Routine body goes here...
	DECLARE s int DEFAULT 0;
	DECLARE p varchar(255);
	DECLARE report CURSOR FOR SELECT table_name FROM information_schema.tables WHERE table_schema='Gchatpro' AND table_name LIKE 'roomlog%' ;
	DECLARE CONTINUE HANDLER FOR NOT FOUND SET s=1;
	open report;
		fetch report into p;
		while s<>1 do
			SET @sqlStr:=CONCAT("DELETE FROM ",p, " WHERE sendtime< date_sub(now(), interval 1 WEEK)");
			PREPARE stmt from @sqlStr;
			EXECUTE stmt;
			DEALLOCATE PREPARE stmt; 
		fetch report into p;
		END while;
	CLOSE report;
END
;;
delimiter ;

-- ----------------------------
-- Procedure structure for test
-- ----------------------------
DROP PROCEDURE IF EXISTS `test`;
delimiter ;;
CREATE PROCEDURE `test`()
BEGIN
	#Routine body goes here...
	DECLARE s int DEFAULT 0;
	DECLARE p varchar(255);
	DECLARE report CURSOR FOR SELECT table_name FROM information_schema.tables WHERE table_schema='Gchatpro' AND table_name LIKE 'roomlog%' ;
	DECLARE CONTINUE HANDLER FOR NOT FOUND SET s=1;
	open report;
		fetch report into p;
		while s<>1 do
			SET @sqlStr:=CONCAT("INSERT INTO  ",p, "(sender, text) VALUES (1, '0')");
			PREPARE stmt from @sqlStr;
			EXECUTE stmt;
			DEALLOCATE PREPARE stmt; 
		fetch report into p;
		END while;
	CLOSE report;
END
;;
delimiter ;

-- ----------------------------
-- Event structure for del_roomlog
-- ----------------------------
DROP EVENT IF EXISTS `del_roomlog`;
delimiter ;;
CREATE EVENT `del_roomlog`
ON SCHEDULE
EVERY '1' WEEK STARTS '2022-04-18 14:31:59'
DO CALL del_oldroomlog()
;;
delimiter ;

SET FOREIGN_KEY_CHECKS = 1;
