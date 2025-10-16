-- phpMyAdmin SQL Dump
-- version 4.5.1
-- http://www.phpmyadmin.net
--
-- Host: 127.0.0.1
-- Generation Time: Oct 16, 2025 at 07:25 AM
-- Server version: 10.1.19-MariaDB
-- PHP Version: 5.6.28

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `wits_app`
--

-- --------------------------------------------------------

--
-- Table structure for table `admin`
--

CREATE TABLE `admin` (
  `id` int(11) NOT NULL DEFAULT '1',
  `admin_key_hash` varchar(255) NOT NULL,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `admin_name` varchar(100) DEFAULT NULL,
  `admin_position` varchar(100) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `admin`
--

INSERT INTO `admin` (`id`, `admin_key_hash`, `updated_at`, `admin_name`, `admin_position`) VALUES
(1, '$2y$10$xg6u29GbjLU7qCsf5tlLYupTZmP2GJs982nXR2s76wewJVN7Z9aVe', '2025-09-09 00:44:24', NULL, NULL);

-- --------------------------------------------------------

--
-- Table structure for table `library_visits`
--

CREATE TABLE `library_visits` (
  `id` int(11) NOT NULL,
  `student_id` varchar(50) NOT NULL,
  `course` varchar(100) NOT NULL,
  `login_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `yearlog` int(11) DEFAULT NULL,
  `year_level` varchar(50) DEFAULT NULL,
  `department` varchar(100) DEFAULT NULL,
  `name` varchar(255) DEFAULT NULL,
  `gender` varchar(50) DEFAULT NULL,
  `status` varchar(50) DEFAULT NULL,
  `photo` varchar(255) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `library_visits`
--

INSERT INTO `library_visits` (`id`, `student_id`, `course`, `login_time`, `yearlog`, `year_level`, `department`, `name`, `gender`, `status`, `photo`) VALUES
(8, '20250107', 'BSEcE', '2025-10-16 03:59:31', 2025, '2', 'CE', 'Cris Tiu', 'Male', '0', 'uploads/20250107_1760572833.png'),
(9, '20250107', 'BSEcE', '2025-10-16 04:04:02', 2025, '2', 'CE', 'Cris Tiu', 'Male', '0', 'uploads/20250107_1760572833.png'),
(10, '20250107', 'BSEcE', '2025-10-16 04:36:28', 2025, '2', 'CE', 'Cris Tiu', 'Male', '0', 'uploads/20250107_1760572833.png');

--
-- Triggers `library_visits`
--
DELIMITER $$
CREATE TRIGGER `set_yearlog_before_insert` BEFORE INSERT ON `library_visits` FOR EACH ROW BEGIN
  SET NEW.yearlog = YEAR(NEW.login_time);
END
$$
DELIMITER ;
DELIMITER $$
CREATE TRIGGER `set_yearlog_before_update` BEFORE UPDATE ON `library_visits` FOR EACH ROW BEGIN
  SET NEW.yearlog = YEAR(NEW.login_time);
END
$$
DELIMITER ;

-- --------------------------------------------------------

--
-- Table structure for table `students`
--

CREATE TABLE `students` (
  `id` int(11) NOT NULL,
  `code` varchar(50) DEFAULT NULL,
  `school_id` varchar(20) NOT NULL,
  `name` varchar(100) NOT NULL,
  `course` varchar(50) DEFAULT NULL,
  `year_level` varchar(50) DEFAULT NULL,
  `department` varchar(50) NOT NULL,
  `time_date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `gender` varchar(10) DEFAULT NULL,
  `status` varchar(20) DEFAULT NULL,
  `photo` varchar(255) DEFAULT NULL,
  `visits` int(11) DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `students`
--

INSERT INTO `students` (`id`, `code`, `school_id`, `name`, `course`, `year_level`, `department`, `time_date`, `gender`, `status`, `photo`, `visits`) VALUES
(1, NULL, '20250101', 'Carlo Canda', 'BSEcE', '3', 'CE', '2025-09-05 11:19:00', 'M', 'Active', NULL, 0),
(2, NULL, '20250102', 'Laurence Tormis', 'BSEcE', '3', 'CE', '2025-09-05 11:18:57', 'M', 'Active', NULL, 0),
(8, '', '20250103', 'Dan Dalandan', 'BSCE', '1', 'CE', '2025-09-06 10:52:29', 'M', 'Active', NULL, 0),
(10, '', '20250104', 'Margarette Demenillo', 'BSA', '4', 'COA', '2025-09-06 14:27:21', 'F', 'Active', NULL, 0),
(11, '', '20250105', 'Fitz Marie Gesta', 'BS in Secondary Education', '3', 'College of Education', '2025-09-06 14:33:01', 'F', 'Active', NULL, 0),
(12, '', '20250106', 'Christine Lumpas', 'BSOT', '3', 'COM', '2025-09-13 08:56:38', 'F', 'Active', NULL, 2),
(178, '', '2020018107', 'ALDEA,THOMAS JOSHUA, GALILA', 'Grade 7', 'Bl. Angelo', 'Junior High School', '2025-09-25 10:12:59', 'M', 'Active', NULL, 0),
(179, '', '2022231501', 'BATAGA,STEVEN, ILANO', 'Grade 7', 'Bl. Angelo', 'Junior High School', '2025-09-25 10:12:59', 'M', 'Active', NULL, 0),
(180, '', '2020008588', 'CRUZ,SEAN BENEDICT, ESPAÃ‘A', 'Grade 7', 'Bl. Anthony', 'Junior High School', '2025-09-25 10:12:59', 'M', 'Active', NULL, 0),
(181, '', '2021007713', 'ARAGON,JHON ALBERT, LOCSIN', 'Grade 7', 'Bl. Anthony', 'Junior High School', '2025-09-25 10:12:59', 'M', 'Active', NULL, 0),
(182, '', '2022232014', 'ASUATIGUE,ARVI JOHN, CANTUBA', 'Grade 7', 'Bl. Anthony', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(183, '', '117602140050', 'CAMBAS,CAINE DAVID VINCENT, FORRO', 'Grade 7', 'Bl. Anthony', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(184, '', '2021007633', 'AMISOLA,DUSTIN MIGUEL, CASIPLE', 'Grade 8', 'Bl. Jerome', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(185, '', '2022231052', 'BUENAFE,KENTH JOHN, BOCAIS', 'Grade 7', 'Bl. Angelo', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(186, '', '2021007631', 'ARINZOL,CHARLES HAYDEN, LUMOGDANG', 'Grade 8', 'Bl. Jerome', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(187, '', '2022231218', 'CLAVEL,JATHNIEL PAUL, ALBAÃ‘EZ', 'Grade 8', 'Bl. Jerome', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(188, '', '2021007638', 'CABARDO,JOHN WILLARD, MATIMTIM', 'Grade 8', 'Bl. Josephine', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(189, '', '2019010758', 'CASIMIRO,ULBBY KAHLEL, GABALES', 'Grade 8', 'Bl. Josephine', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(190, '', '2022231080', 'BLANCA,KEN ALDRINE, OPJER', 'Grade 7', 'Bl. Angelo', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(191, '', '2020008742', 'TIONKO,ALYANAH JULIENNE, BASTIERO', 'Grade 8', 'Bl. Josephine', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(192, '', '2020019122', 'TOLOSA,ALEXA MICAH, SAMSON', 'Grade 8', 'Bl. Josephine', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(193, '', '2020008642', 'VILLALUNA,MATHENA JIMELLA, CORDERO', 'Grade 8', 'Bl. Josephine', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(194, '', '2021007669', 'ZARAGOZA,IVANA MATHEA, LUCENIO', 'Grade 8', 'Bl. Josephine', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(195, '', '2021007716', 'ABAD,JAMES EDUARD, FLORES', 'Grade 8', 'Bl. Julia', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(196, '', '2019008747', 'AGUSTIN,DEVAN ALVIC, TORREVERDE', 'Grade 8', 'Bl. Julia', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(197, '', '2019010949', 'AMPER,JONICK KOBE, PABILLO', 'Grade 8', 'Bl. Julia', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(198, '', '2022231165', 'BERNIL,EDEN VAN, BERNASOL', 'Grade 8', 'Bl. Julia', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(199, '', '2022231221', 'SANG-ALAN,LEIGHANNA SHANEN, MENDOZA', 'Grade 8', 'Bl. Julia', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(200, '', '2021007701', 'PELOBELLO,GEENIE, FRANCISCO', 'Grade 8', 'Bl. Julia', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(201, '', '2021007644', 'AYUPAN,JOSE MARI ANGELO, BUCANA', 'Grade 8', 'Bl. Julia', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(202, '', '2021007517', 'TABUL,J-RIA DANIELLE, INDENCIA', 'Grade 8', 'Bl. Julia', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(203, '', '2022231399', 'TINOKO,LOUISE JADE, JAGNA-AN', 'Grade 8', 'Bl. Julia', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(204, '', '2022231115', 'ALONSAGAY,KERX DANIEL, TOBOSO', 'Grade 9', 'Bl. Magdalene', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(205, '', '2022231109', 'MAGTOTO,EDWYN JULIETTE, IBAÃ‘EZ', 'Grade 9', 'Bl. Magdalene', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(206, '', '2022231013', 'MORENO,LARA SOFIA, ESCANLAR', 'Grade 9', 'Bl. Magdalene', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(207, '', '2020014084', 'AURESTILA,KYRIAN EZIEKEL, SILVERIO', 'Grade 9', 'Bl. Magdalene', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(208, '', '2021007652', 'PAILANAN,ELLA RAYE, DE LOS SANTOS', 'Grade 9', 'Bl. Magdalene', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(209, '', '2021007566', 'PALMOS,BEATRICE, GRANPEÃ‘AS', 'Grade 9', 'Bl. Magdalene', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(210, '', '2022231401', 'PIANDO,CAIRISTIONA FJORD, MACARAIG', 'Grade 9', 'Bl. Magdalene', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(211, '', '20222312102', 'SIMPLICIO,MARIA CHRISNA, BACISLAO', 'Grade 9', 'Bl. Magdalene', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(212, '', '2020008641', 'AGAN,MYTHOS EINSTEIN, SOLIVA', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(213, '', '2019008833', 'BUGNA,ALLAN DOMINIC, ESGRINA', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(214, '', '2020000354', 'BALINGIT,ANDREW THEODORE, AMBRAD', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(215, '', '2019008534', 'DEMERIN,JEBERT CEASAR, BABIERA', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(216, '', '2020015106', 'GALIDO,ART DARELLE, ORTEGA', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(217, '', '2019018915', 'GOMEZ,BENN DENZYL, BADIANGO', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(218, '', '2019008636', 'LOSBAÃ‘ES,JEREMY THEODORE, ALCAYAGA', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(219, '', '2019018507', 'MALAZZAB,JOAQUIN CARLO, BALCARSE', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(220, '', '2021007548', 'MENDOZA,JOHN ERIC, HISMANA', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(221, '', '2019008658', 'MOLINOS,RAYXELL KYNN, YAP', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(222, '', '2020014090', 'MONTUYA,RAINIER KRISTOPHER, ERFE', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(223, '', '2019010967', 'VARON,ALLEN BENEDICTH, PORRAS', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(224, '', '2020014085', 'VERA CRUZ,SHOLEM ASH, DEMORITO', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(225, '', '2019011006', 'AZUELO,ANDREA JULIENNE, LAVENTE', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(226, '', '2020014091', 'BAÃ‘ARES,JOLEANNA YZABELLE, DIONSON', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(227, '', '2020014083', 'ABUTANATIN,JOSHUA BENJAMIN, REBUSQUILLO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(228, '', '2019010800', 'BENDALI-AN,MARY DOMINIC, PUGA', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(229, '', '2020019125', 'AMARANTE,ADRIAN KYLE, POSECION', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(230, '', '2019008952', 'BERGANIO,CARLO ANGELO, ORDOÃ‘EZ', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(231, '', '2020018106', 'CARANCIO,IOAN JAMES, DIOCENA', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(232, '', '2020014102', 'KRELL,VALENTINO, GETULLE', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(233, '', '2020015108', 'CORDERO,MARTIN CHRISTOPHER, DE ASIS', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(234, '', '2020000367', 'LAJO,DENNIS, DELFIN', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(235, '', '2020014086', 'MONEGRO,JUSTINE, BESANA', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(236, '', '2019010948', 'PALLADA,GIANNI GAETANO, CASTILLO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(237, '', '2020018108', 'PELOBELLO,ANGELO, FRANCISCO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(238, '', '2022232012', 'BELOSO,RHIANNA JOY, CARILLO', 'Grade 9', 'Bl. Maria Teresa', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(239, '', '2019008680', 'RABACA,GERARD ULRIC, CUDILLA', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(240, '', '2019008948', 'SEDUCO,LERIC, PEREZ', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(241, '', '2020014089', 'SOLTIS,STAN ANGELO, CALIBANG', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(242, '', '2019010902', 'SUERTE,CHAD MARTIN, SUBEBE', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(243, '', '2020008635', 'VILLANUEVA,LUIS, LIMBANG', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(244, '', '2020014082', 'ABAYGAR,CHANEL VEAN, PILLADO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(245, '', '2019010763', 'ANATAN,CHERRYLAN, CASTRO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(246, '', '2020014105', 'ARINZOL,MARY CHARLIZE, LUMOGDANG', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(247, '', '2020015110', 'CUBERO,ESTRELLA MARIE, DAVAO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(248, '', '2019009030', 'CONCEPCION,KATE, TINGSON', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(249, '', '2020000527', 'BERANGEL,RUSLEE QUEEN ALTHEA, NAVITA', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(250, '', '2019008912', 'GUEVARRA,RAESHA, GARRUCHO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(251, '', '2020014092', 'ESTOS,YVONNE LORIEL, VERZO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(252, '', '2021007667', 'CAJURAO,MADELEINE PRINCESS, NAVARRO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(253, '', '2020014101', 'HUBAG,ALLYSHA YZAVELLE, PORRAS', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(254, '', '2020009013', 'JACINTOS,JEREMAE, GUMBAN', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(255, '', '2019008503', 'JAUDINES,ANDREA LOUISE, SARGENTO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(256, '', '2020015092', 'MANA-AY,MANILYN GRACE, NIEMBRA', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(257, '', '2019010867', 'MIRALLES,MA FRANCHESCA, SAUL', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(258, '', '2019009039', 'PALCON,ARHIANNEY CHRISVIAN, BRIONES', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(259, '', '2020008871', 'PEREZ,JACQUELYN ANNE, VISITACION', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(260, '', '2020014073', 'SIPOLE,NORIANNE ANGELA, CASUYON', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(261, '', '2020008370', 'SUCGANG,GELLA MAYE, OBERIANO', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(262, '', '2019008605', 'SUMAYO,CYRA CAMILLE, TABUENA', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(263, '', '2020000584', 'TANGUAN,FLORDETTE ANN BEA, JANDUCAYAN', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(264, '', '2020014095', 'VELASCO,SOPHIA CLAIRE, REGIDOR', 'Grade 9', 'Bl. Mother of Consolation', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(265, '', '2019010927', 'ALVIZ,JIMRYX KYLE, LUCERO', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(266, '', '2021007587', 'BALAIROS,GLIAN DREW, BARRIDA', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(267, '', '2019008786', 'BALLADARES,JAN MICAH, FAX', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(268, '', '2020008469', 'CORDERO,ADRIAN BON, JALANDONI', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(269, '', '2019008440', 'FIGUEROA,ROBERT PAUL, CARAGAN', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(270, '', '201901146', 'DEL ROSARIO,VINCE RYAN, DEOCAMPO', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(271, '', '2020008578', 'HELER,FRANCOIS CHRIS, NOBLEZA', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(272, '', '2019010849', 'GRECIA,LAWRENCE CAESAR, PARO', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(273, '', '2019008394', 'IDAO,KIRBY SHAWN, BALGOS', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(274, '', '2019008562', 'CLARITO,JERO, SEPANTON', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(275, '', '2019008513', 'INAYAN,CHRISTIAN JOHN, JAMERO', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(276, '', '2019008665', 'CLARITO,YUAN RESHI, CARASCO', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(277, '', '440003150051', 'LAZARTE,MARLOWE ANGELO, LATORZA', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(278, '', '2021007656', 'SALAYA,MIGUEL, REBUENO', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(279, '', '2019008518', 'ROBLES,TERYL KEITH, BAYU', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(280, '', '2020008732', 'PASADILLA,CLYDE EDMON, COO', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(281, '', '2019888676', 'SEPE,KEETH ANDREI, ARDENA', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(282, '', '2019008474', 'LOPEZ,NICOLE DENISE, GRANPEÃ‘AS', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(283, '', '2019008462', 'MALDECER,MARIA REBECCA, -', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(284, '', '2019009033', 'OQUENDO,SABRINA ROSHE, -', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(285, '', '2019008428', 'SALARDA,CHRIZZAMIL, CASTILLO', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(286, '', '2019008596', 'TURGO,MAIKA ABIGAIL, MAJADUCON', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(287, '', '2019018624', 'VICENTE,VENICE, CASIPLE', 'Grade 10', 'Bl. Elias', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(288, '', '20222312108', 'ALUNAN,ANGELO DAVE, CONSTANTINO', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(289, '', '2022231023', 'COLUMBRES,ANDREJOHN ISAAC, TABIRARA', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(290, '', '2019010752', 'CONDINO,RONALD ANDREI, TEMPOROSA', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(291, '', '2022231062', 'CAMPANIEL,SEAN LOUVER, CATEDRAL', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(292, '', '2020008817', 'GERNADE,VINCENT TROY, -', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(293, '', '202220231251', 'GREGORIO,JOHN BENEDICT, CASAS', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(294, '', '2022231227', 'GICOLE,PRINCE JULZE, CENIS', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(295, '', '2019008552', 'LOPEZ,DAVID ANDREW, SILLA', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(296, '', '2020008643', 'MONTECERIN,VINCE MATTHEW, TILLO', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(297, '', '202220231301', 'MIRANDA,GARETTE OSIAS, DEOCAMPO', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(298, '', '2020008371', 'NAPUD,KRYZ TYRAEL, VILLAVECENCIO', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(299, '', '2022231186', 'LEDESMA,RUSS PIERRE JOSEPH, VARON', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(300, '', '2022231526', 'PONO,IKE MARCUS, TERANA', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(301, '', '2022231306', 'PANES,GIAN FAISAL, SUAREZ', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(302, '', '2019010751', 'SIMPAS,SASKE JAN, SORNILLO', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(303, '', '2019008959', 'SORIA,NOELAN, BOLIVAR', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(304, '', '2019008988', 'SUARNABA,CHAD KOBI, SUMERAS', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(305, '', '2020008590', 'TABUL,DION JASPER, INDENCIA', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(306, '', '2019008809', 'TERNURA,REUEL ZETH, BALAJADIA', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(307, '', '2022231161', 'UMADHAY,MARK ANDREW, GALFO', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(308, '', '2022231143', 'ALMERIA,LOUCRESCE ARRIANE, LEGAYADA', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(309, '', '2022231503', 'BANTOLO,PRECIOUS LARA, AGTING', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(310, '', '2021007691', 'CAMANDERO,HANNAH MARIE, GALLO', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(311, '', '2019008522', 'BISCAS,KHATRINA, OMBI-ON', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(312, '', '2019010971', 'ALVAREZ,ATHALIE KEFFY RENIZE, CABARDO', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(313, '', '2020000610', 'DALIPE,ARIANNA ASHLEY, ULEP', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(314, '', '20222312110', 'ESMAYA,PS LOVELY GRACE, LINAO', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(315, '', '2021007560', 'ESTRADA,YELAH BLYTHE, BIÃ‘AS', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(316, '', '2021007617', 'HIERRO,RENZYL, YALUNG', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(317, '', '2022231126', 'PARINA,ARRACREXIA REYGIN, CASTRE', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(318, '', '2019010818', 'PABILLARAN,ODESSA ANDREA, IBAÃ‘EZ', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(319, '', '2022232056', 'PAMA,JEYAH ELIZA, ABANGAN', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(320, '', '2019008958', 'SADIWA,ASHLEY NICOLE, DIERON', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(321, '', '442574150145', 'CORDERO,ELYSE, BILIRAN', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(322, '', '2022231074', 'TITULAR,ELIYAH KEIRA, PARREÃ‘AS', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(323, '', '2019008972', 'VILLALOBOS,JULIENNE YSABELLE, ANG', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(324, '', '19008752', 'BERMEJO,JOHN ARVIE, HIPOLITO', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(325, '', '2022231293', 'MAHANDOG,ALLYZA SAMANTHA, CARIÃ‘O', 'Grade 10', 'Bl. Frederick', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(326, '', '2019008813', 'CABARACA,JOHNNIEL ANDRE, CAMPONION', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(327, '', '2019010883', 'CARTEL,CHAD ERNEST, VILLARETE', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(328, '', '2019010953', 'DE GUZMAN,KEITH MATHEW, ALINGASA', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(329, '', '2022231394', 'DE HONOR,SIAN RYVER, ALORRO', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(330, '', '2019008422', 'FULIGA,ALIREI DREW, BEBOSO', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(331, '', '2019008946', 'CATOTO,JOLAND GABRIEL, QUINTRO', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(332, '', '2020008602', 'GANZON,SHAUN COURVIE, GEROCHI', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(333, '', '2020010849', 'GARCIA,JOAQUIN VISH, PLANILLA', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(334, '', '2019008386', 'JABLO,DAVE RYAN, TALUSAN', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(335, '', '2019014062', 'HUERVANA,JOSHUA ARVIN, PEREZ', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(336, '', '2020000562', 'HALLARA,ARRON KIAN BENEDICT, MANA-AY', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(337, '', '2019009741', 'HILADO,JOEFF BRYAN, LUBATON', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'F', '', NULL, 0),
(338, '', '2022238502', 'JULIAGA,GEBRON, SALCEDO', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(339, '', '2019010749', 'LEMANA,CHASE DEAN, CATALAN', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(340, '', '2019008819', 'KIM,HAJIN VICTOR BLYTHE, LASTIMOSO', 'Grade 10', 'Bl. Gonzalo', 'Junior High School', '2025-09-25 10:12:59', 'M', '', NULL, 0),
(341, '', '20250107', 'Cris Tiu', 'BSEcE', '2', 'CE', '2025-10-16 08:00:33', 'Male', '0', 'uploads/20250107_1760572833.png', 10);

-- --------------------------------------------------------

--
-- Table structure for table `visitors`
--

CREATE TABLE `visitors` (
  `id` int(11) NOT NULL,
  `name` varchar(100) NOT NULL,
  `company` varchar(255) NOT NULL,
  `contact` varchar(50) DEFAULT NULL,
  `purpose` varchar(255) DEFAULT NULL,
  `time_in` datetime DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `visitors`
--

INSERT INTO `visitors` (`id`, `name`, `company`, `contact`, `purpose`, `time_in`) VALUES
(1, 'justine', 'wits', '09876556676', 'visit', '2025-09-12 14:33:11'),
(2, 'Alyssa Kyle Canete', 'WITS', '09477869352', 'hatdog', '2025-10-07 12:30:40');

--
-- Indexes for dumped tables
--

--
-- Indexes for table `admin`
--
ALTER TABLE `admin`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `library_visits`
--
ALTER TABLE `library_visits`
  ADD PRIMARY KEY (`id`),
  ADD KEY `student_id` (`student_id`);

--
-- Indexes for table `students`
--
ALTER TABLE `students`
  ADD PRIMARY KEY (`id`),
  ADD UNIQUE KEY `school_id` (`school_id`);

--
-- Indexes for table `visitors`
--
ALTER TABLE `visitors`
  ADD PRIMARY KEY (`id`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `library_visits`
--
ALTER TABLE `library_visits`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=11;
--
-- AUTO_INCREMENT for table `students`
--
ALTER TABLE `students`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=342;
--
-- AUTO_INCREMENT for table `visitors`
--
ALTER TABLE `visitors`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;
--
-- Constraints for dumped tables
--

--
-- Constraints for table `library_visits`
--
ALTER TABLE `library_visits`
  ADD CONSTRAINT `library_visits_ibfk_1` FOREIGN KEY (`student_id`) REFERENCES `students` (`school_id`);

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
