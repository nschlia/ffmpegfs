

<!DOCTYPE html>
<html lang="en">
   <head>
      <title>Transcoder Test</title>
      <meta http-equiv="X-UA-Compatible" content="IE=edge">
      <meta charset="UTF-8">
      <style>
         body {
         font-family: Arial, Helvetica, sans-serif;
         }
         td {
         vertical-align: top;
         padding: 0px;
         }
         table {
         border-spacing: 0px;
         border-collapse: separate;
         }
      </style>
      <script language="javascript">
         function doplay(url) {
         var video = document.getElementById('video');

         d = new Date();

         video.src =  url + "?" + d.getTime();

         video.load();
         video.play();
         }

         function get_filesize(url, callback) {
         var xhr = new XMLHttpRequest();
         xhr.open("HEAD", url, true); // Notice "HEAD" instead of "GET",
         //  to get only the header
         xhr.onreadystatechange = function() {
         if (this.readyState == this.DONE) {
         callback(parseInt(xhr.getResponseHeader("Content-Length")));
         }
         };
         xhr.send();
         }

         function dopreload(url) {
         get_filesize(url, function(size) {
         alert("The size is: " + size + " bytes.");
         });
         }

         function canplay(type) {

         var video = document.getElementById('video');
         var opt = document.createElement("option");
         opt.text = type + ": " + video.canPlayType(type);
         //opt.value =
         document.getElementById("ListBox1").options.add(opt);
         }

         window.onload = function()
         {
         canplay('video/mp4');
         canplay('audio/mp4');
         canplay('video/ogg');
         canplay('audio/ogg');
         canplay('video/webm');
         canplay('video/mpeg');
         canplay('audio/mp3');
         canplay('audio/flac');
         };

      </script>
   </head>
   <body>
      <table>
         <tr>
            <td>
               <!--
                  <video id="video" class="videowindow" controls poster="img/biglogo.png" width="720" height="560">
                  -->
               <video id="video" class="videowindow" controls width="720" height="560">
                  Your browser does not support the video tag.
               </video>
            </td>
            <td>
               <table>
                  <?php		  
function reverse_strrchr($haystack, $needle)
{
    $pos = strrpos($haystack, $needle);
    if ($pos === false) {
        return $haystack;
    }
    return substr($haystack, 0, $pos);
}

$dir = './';
$files = array_filter(scandir($dir), function($item)
{
    global $dir;
    if (!preg_match("/\.mp[34]|\.ogg/i", $item)) {
        return 0;
    }
    return !is_dir($dir . $item);
});

$dirs = array_filter(scandir($dir), function($item)
{
    global $dir;
    if ($item == "." || $item == "..") {
        return 0;
    }
    return is_dir($dir . $item);
});

$dir  = dirname($_SERVER['PHP_SELF']);
$back = reverse_strrchr($dir, "/", 1);
if ($back != "") {
    echo "<tr><td>";
    echo "<a href=\"" . utf8_encode($back) . "\">Go back</a>";
    echo "</td>\n<td>";
    echo "</td></tr>\n";
    
    echo "<tr><td height=20>";
    echo "</td>\n<td>";
    echo "</td></tr>\n";
}

foreach ($dirs as &$value) {
    echo "<tr><td>";
    echo "<a href=\"" . rawurlencode($value) . "\">" . utf8_encode($value) . "</a>&nbsp;";
    echo "</td>\n<td>\n";
    echo "</td></tr>\n";
}

echo "<tr><td height=20>";
echo "</td><td>";
echo "</td></tr>";

foreach ($files as &$value) {
    echo "<tr><td>";
    echo "<a href=\"javascript:doplay('" . rawurlencode($value) . "')\">" . utf8_encode($value) . "</a>&nbsp;";
    echo "</td>\n<td>";
    echo "<a href=\"javascript:dopreload('" . rawurlencode($value) . "')\">Preload</a>";
    echo "</td></tr>\n";
}
                     ?>
                  <tr>
                     <td>
                        <br />
                        <b>Supported file types</b>
                        <br />
                        <select id="ListBox1" size="8" style="width: 100%; height: 50%">
                        </select>
                     </td>
                  </tr>
               </table>
            </td>
         </tr>
      </table>
   </body>
</html>

