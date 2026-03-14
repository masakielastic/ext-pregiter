--TEST--
preg_iter_ex supports PREG_UNMATCHED_AS_NULL
--EXTENSIONS--
pregiter
--FILE--
<?php
$it = preg_iter_ex('/(?P<x>a)?(?P<y>b)?/', 'b', PREG_OFFSET_CAPTURE | PREG_UNMATCHED_AS_NULL);
foreach ($it as $match) {
    var_export($match);
    echo "\n";
}
?>
--EXPECT--
array (
  0 => 
  array (
    0 => 'b',
    1 => 0,
  ),
  'x' => 
  array (
    0 => NULL,
    1 => -1,
  ),
  1 => 
  array (
    0 => NULL,
    1 => -1,
  ),
  'y' => 
  array (
    0 => 'b',
    1 => 0,
  ),
  2 => 
  array (
    0 => 'b',
    1 => 0,
  ),
)
array (
  0 => 
  array (
    0 => '',
    1 => 1,
  ),
  'x' => 
  array (
    0 => NULL,
    1 => -1,
  ),
  1 => 
  array (
    0 => NULL,
    1 => -1,
  ),
  'y' => 
  array (
    0 => NULL,
    1 => -1,
  ),
  2 => 
  array (
    0 => NULL,
    1 => -1,
  ),
)
