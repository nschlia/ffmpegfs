<!DOCTYPE html>
<html lang="en">
   <head>
      <meta charset="utf-8">
      <title>Transcoder Test</title>
      <meta http-equiv="X-UA-Compatible" content="IE=edge">
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
                         echo "<a href=\"javascript:dopreload('" . $value . "')\">Preload</a>";
                         echo "</td></tr>";
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

