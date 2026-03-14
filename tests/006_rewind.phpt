--TEST--
preg_iter_ex can rewind and iterate twice
--EXTENSIONS--
pregiter
--FILE--
<?php
$it = preg_iter_ex('/[a-z]+/', 'abc def');
foreach ($it as $i => $match) {
    echo $i, ':', $match[0], "\n";
}
echo "--\n";
foreach ($it as $i => $match) {
    echo $i, ':', $match[0], "\n";
}
?>
--EXPECT--
0:abc
1:def
--
0:abc
1:def
