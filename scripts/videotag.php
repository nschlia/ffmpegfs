<!DOCTYPE html>
<html lang="en">
   <head>
      <meta charset="utf-8">
      <title>Transcoder Test</title>
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
         	var mp4_src = document.getElementById("mp4_src");
         	var ogg_src = document.getElementById("ogg_src");
         
         d = new Date();
         
         mp4_src.src = url + "?" + d.getTime();
         
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
         
         function test(url) {
         get_filesize(url, function(size) {
         		alert("The size is: " + size + " bytes.");
         });
         }
         
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
                  <source id="mp4_src" src="" type="video/mp4">
                  <source id="ogg_src" src="" type="video/ogg">
               </video>
            </td>
            <td>
               <table>
                  <?php
                     $dir    = './';
                     $files = array_filter(scandir($dir), function($item) {
                     	global $dir;
                     	if (!preg_match("/\.mp[34]|\.ogg/i", $item)) {
                     		return 0;
                     	}
                         	return !is_dir($dir . $item);
                     });
                     
                     foreach ($files as &$value) {
                         echo "<tr><td>";
                         echo "<a href=\"javascript:doplay('" . $value . "')\">" . $value . "</a>&nbsp;";
                         echo "</td><td>";
                         echo "<a href=\"javascript:test('" . $value . "')\">Preload</a>";
                         echo "</td></tr>";
                     }
                     
                     ?>
               </table>
            </td>
         </tr>
      </table>
   </body>
</html>
