// Скрипт "Проверить качество иллюстраций" для редактора FBE
// version 4.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для проверки всех бинарных файлов иллюстраций в fb2 документе:
// Скрипт анализирует и выводит отчет для всех иллюстраций, с указанием проблемных бинарников:
// - размеры в пикселях (ширина и высота)
// - вес файлов
// - разрешение (DPI)
// - цветовую модель (например, CMYK для JPEG = проблема)
// - глубину цвета (информативно для PNG)
// - количество цветов для Indexed color PNG
// - избыточную цветность (RGB/RGBA, но фактически grayscale)
// - наличие слоев (RGBA/grayscale+alpha — рекомендуется схлопнуть)
// - неподдерживаемые форматы (всё кроме JPG и PNG)
// Сравнивает с заданными настройками и сообщает о проблемных файлах.
// При наличии проблемных файлов скрипт сохраняет их вместе с txt отчетом
// в папку Problem_images рядом с текущим fb2 документом.
// Получение полной информации по файлам занимает некоторое, иногда существенное, время.
// Это зависит от мощности конкретного компьютера.
// Зависимость от кол-ва файлов нелинейная. Беспроблемные файлы обрабатываются гораздо быстрее.
// На анализ 120 бинарников затрачивается около 70 секунд, 400 файлов - около 5 минут.
// Дольше всего обрабатываются файлы в цветовом пространстве CMYK, сохраненные через Photoshop.
// Никаких изменений в документе скрипт не производит.

// version 4.0, 30.08.2026
//======================================

// Глобальная функция для форматирования размера файла
function formatFileSize(bytes) {
    if (bytes <= 0) return "0 байт";
    if (bytes < 1024) return bytes + " байт";
    if (bytes < 1024 * 1024) {
        var kb = Math.round(bytes / 1024);
        return kb + " Кб";
    } else {
        var mb = Math.round(bytes / (1024 * 1024) * 100) / 100;
        return mb + " Мб";
    }
}

// Глобальная функция для правильного согласования "цвет/цвета/цветов"
function getColorWord(count) {
    if (count % 10 == 1 && count % 100 != 11) {
        return "цвет";
    } else if (count % 10 >= 2 && count % 10 <= 4 && (count % 100 < 10 || count % 100 >= 20)) {
        return "цвета";
    } else {
        return "цветов";
    }
}

function Run() {
    var scriptName = "Проверить качество иллюстраций";
    var version = "4.0";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать полную статистику, 0 - тихий режим
    // В тихом режиме скрипт молча выполняет проверку и показывает
    // только краткое сообщение о результатах (проблемы, отчёт, время).
    var showStatistics = 1;
    
    // Показывать полный отчет по всем бинарникам в окне
    var showFullReport = 0;
    
    // Всегда писать файл отчета: 0 - только при проблемах, 1 - всегда
    var alwaysWriteReport = 0;
    
    // Считать RGB/RGBA (но фактически grayscale) бинарники проблемными файлами
    var treatExcessColorAsProblem = 1; // 0 - нет, 1 - да
    
    // Считать RGBA (со слоями) бинарники проблемными файлами
    var treatLayersAsProblem = 1; // 0 - нет, 1 - да
    
    // Считать grayscale+alpha (со слоями) бинарники проблемными файлами
    var treatGrayAlphaAsProblem = 1; // 0 - нет, 1 - да
    
    // Максимальное отклонение R/G/B для определения grayscale
    // (при сжатии JPEG/PNG могут быть небольшие отклонения от серого)
    var maxColorDeviation = 10; // допустимое отклонение (0-255)
    
    // Максимальный процент цветных пикселей (отклонение > maxColorDeviation)
    // для определения изображения как grayscale
    var maxColorPixelPercent = 1; // процент (0-100)
    
    // Шаг проверки пикселей (1 - каждый пиксель, 3 - каждый 3-й и т.д.)
    // Больше шаг = быстрее проверка, но менее точно
    var pixelStep = 3;
    
    // Стандартные разрешения (DPI)
    var standardJpgDpi = 72;     // Стандартное разрешение для JPG
    var standardPngDpiMin = 72;  // Минимальное стандартное для PNG
    var standardPngDpiMax = 150; // Максимальное стандартное для PNG
    
    // Максимальные размеры в пикселях
    var maxWidth = 2000;   // Максимальная ширина
    var maxHeight = 3000;  // Максимальная высота
    
    // Максимальное количество проблемных файлов для показа в MsgBox
    var maxShowInMsgBox = 10;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var startTime = 0;
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    // Функция получения расширения файла (с учетом суффиксов _N для дубликатов)
    function getFileExtension(fileName) {
        if (!fileName) return "";
        
        var baseName = fileName;
        var suffixMatch = fileName.match(/_\d+$/);
        if (suffixMatch) {
            baseName = fileName.substring(0, fileName.length - suffixMatch[0].length);
        }
        
        var lastDot = baseName.lastIndexOf(".");
        if (lastDot == -1) return "";
        return baseName.substring(lastDot + 1).toLowerCase();
    }
    
    // Функция получения папки с документом
    function getDocumentPath() {
        var folderPath = "";
        try {
            folderPath = window.external.GetDocumentDirectory();
            if (folderPath != "") return folderPath;
        } catch(e) {}
        return "";
    }
    
    // Функция получения имени файла документа
    function getDocumentFileName() {
        var fileName = "";
        try {
            fileName = window.external.GetDocumentFileName();
        } catch(e) {}
        return fileName;
    }
    
    // Функция получения полного пути к документу
    function getDocumentFilePath() {
        var filePath = "";
        try {
            filePath = window.external.GetDocumentFilePath();
        } catch(e) {}
        return filePath;
    }
    
    // Функция получения HEX данных файла через PowerShell
    function getFileHexViaPowerShell(filePath, maxBytes) {
        var hexData = "";
        var tempOutput = "C:\\__fbe_hex_result.txt";
        
        try {
            var shell = new ActiveXObject("WScript.Shell");
            var fso = new ActiveXObject("Scripting.FileSystemObject");
            
            var psCmd = "powershell -ExecutionPolicy Bypass -command \"$b = [System.IO.File]::ReadAllBytes('" + filePath.replace(/\\/g, "\\\\") + "'); $r = ''; $n = [Math]::Min(" + maxBytes + ", $b.Length); for ($i = 0; $i -lt $n; $i++) { $r += $b[$i].ToString('X2') + ' ' }; Write-Output $r\" > \"" + tempOutput + "\" 2>&1";
            
            shell.Run(psCmd, 0, true);
            
            if (fso.FileExists(tempOutput)) {
                var file = fso.OpenTextFile(tempOutput, 1, false, -1);
                hexData = file.ReadLine();
                file.Close();
                try { fso.DeleteFile(tempOutput); } catch(e) {}
            }
        } catch (e) {}
        
        return hexData;
    }
    
    // Функция поиска последнего SOF маркера (для CMYK JPEG)
    function getLastSofViaPowerShell(filePath) {
        var result = "";
        var tempOutput = "C:\\__fbe_last_sof.txt";
        
        try {
            var shell = new ActiveXObject("WScript.Shell");
            var fso = new ActiveXObject("Scripting.FileSystemObject");
            
            var psCmd = "powershell -ExecutionPolicy Bypass -command \"$b = [System.IO.File]::ReadAllBytes('" + filePath.replace(/\\/g, "\\\\") + "'); $lastSof = ''; for ($i = 0; $i -lt $b.Length - 10; $i++) { if ($b[$i] -eq 0xFF -and ($b[$i+1] -eq 0xC0 -or $b[$i+1] -eq 0xC2)) { $lastSof = $i.ToString() + ':' + $b[$i+4].ToString() + ':' + $b[$i+9].ToString() } }; Write-Output $lastSof\" > \"" + tempOutput + "\" 2>&1";
            
            shell.Run(psCmd, 0, true);
            
            if (fso.FileExists(tempOutput)) {
                var file = fso.OpenTextFile(tempOutput, 1, false, -1);
                result = file.ReadLine();
                file.Close();
                try { fso.DeleteFile(tempOutput); } catch(e) {}
            }
        } catch (e) {}
        
        return result;
    }
    
    // Функция проверки фактической grayscale через PowerShell
    // (используется только для RGB/RGBA файлов)
    function checkActualGrayscale(filePath) {
        var result = "";
        var tempOutput = "C:\\__fbe_gray_check.txt";
        
        try {
            var shell = new ActiveXObject("WScript.Shell");
            var fso = new ActiveXObject("Scripting.FileSystemObject");
            
            var psCmd = "powershell -ExecutionPolicy Bypass -command \"Add-Type -AssemblyName System.Drawing; $bytes = [System.IO.File]::ReadAllBytes('" + filePath.replace(/\\/g, "\\\\") + "'); $ms = New-Object System.IO.MemoryStream(,$bytes); $img = [System.Drawing.Image]::FromStream($ms); $bmp = New-Object System.Drawing.Bitmap($img); $isGray = 'True'; for ($y = 0; $y -lt $bmp.Height; $y += " + pixelStep + ") { for ($x = 0; $x -lt $bmp.Width; $x += " + pixelStep + ") { $p = $bmp.GetPixel($x, $y); if ($p.A -gt 10) { $d1 = [Math]::Abs($p.R - $p.G); $d2 = [Math]::Abs($p.G - $p.B); $dev = [Math]::Max($d1, $d2); if ($dev -gt " + maxColorDeviation + ") { $isGray = 'False'; break } } }; if ($isGray -eq 'False') { break } }; $bmp.Dispose(); $img.Dispose(); $ms.Dispose(); Write-Output $isGray\" > \"" + tempOutput + "\" 2>&1";
            
            shell.Run(psCmd, 0, true);
            
            if (fso.FileExists(tempOutput)) {
                var file = fso.OpenTextFile(tempOutput, 1, false, -1);
                result = file.ReadLine();
                file.Close();
                try { fso.DeleteFile(tempOutput); } catch(e) {}
            }
        } catch (e) {}
        
        return result;
    }
    
    // Функция чтения DPI из EXIF (по offset для RATIONAL)
    function getExifDpi(bytes, exifStart) {
        var dpi = -1;
        var tiffStart = exifStart + 6;
        
        if (tiffStart + 8 > bytes.length) return dpi;
        
        var byteOrder = "";
        if (bytes[tiffStart] == 0x49 && bytes[tiffStart+1] == 0x49) {
            byteOrder = "II";
        } else if (bytes[tiffStart] == 0x4D && bytes[tiffStart+1] == 0x4D) {
            byteOrder = "MM";
        } else {
            return dpi;
        }
        
        var magic = (bytes[tiffStart+2] << 8) | bytes[tiffStart+3];
        if (magic != 0x2A) return dpi;
        
        var ifdOffset = 0;
        if (byteOrder == "II") {
            ifdOffset = bytes[tiffStart+4] | (bytes[tiffStart+5] << 8) | (bytes[tiffStart+6] << 16) | (bytes[tiffStart+7] << 24);
        } else {
            ifdOffset = (bytes[tiffStart+4] << 24) | (bytes[tiffStart+5] << 16) | (bytes[tiffStart+6] << 8) | bytes[tiffStart+7];
        }
        
        var ifdStart = tiffStart + ifdOffset;
        
        if (ifdStart + 2 > bytes.length) return dpi;
        
        var numEntries = 0;
        if (byteOrder == "II") {
            numEntries = bytes[ifdStart] | (bytes[ifdStart+1] << 8);
        } else {
            numEntries = (bytes[ifdStart] << 8) | bytes[ifdStart+1];
        }
        
        var xResolution = -1;
        var resolutionUnit = -1;
        
        for (var entry = 0; entry < numEntries && entry < 50; entry++) {
            var entryOffset = ifdStart + 2 + entry * 12;
            
            if (entryOffset + 12 > bytes.length) break;
            
            var tag = 0;
            var type = 0;
            var count = 0;
            var valueOffset = 0;
            
            if (byteOrder == "II") {
                tag = bytes[entryOffset] | (bytes[entryOffset+1] << 8);
                type = bytes[entryOffset+2] | (bytes[entryOffset+3] << 8);
                count = bytes[entryOffset+4] | (bytes[entryOffset+5] << 8) | (bytes[entryOffset+6] << 16) | (bytes[entryOffset+7] << 24);
                valueOffset = bytes[entryOffset+8] | (bytes[entryOffset+9] << 8) | (bytes[entryOffset+10] << 16) | (bytes[entryOffset+11] << 24);
            } else {
                tag = (bytes[entryOffset] << 8) | bytes[entryOffset+1];
                type = (bytes[entryOffset+2] << 8) | bytes[entryOffset+3];
                count = (bytes[entryOffset+4] << 24) | (bytes[entryOffset+5] << 16) | (bytes[entryOffset+6] << 8) | bytes[entryOffset+7];
                valueOffset = (bytes[entryOffset+8] << 24) | (bytes[entryOffset+9] << 16) | (bytes[entryOffset+10] << 8) | bytes[entryOffset+11];
            }
            
            if (tag == 0x011A && type == 5 && count == 1) {
                var dataPos = tiffStart + valueOffset;
                
                if (dataPos + 8 <= bytes.length) {
                    var num = 0;
                    var den = 0;
                    
                    if (byteOrder == "II") {
                        num = bytes[dataPos] | (bytes[dataPos+1] << 8) | (bytes[dataPos+2] << 16) | (bytes[dataPos+3] << 24);
                        den = bytes[dataPos+4] | (bytes[dataPos+5] << 8) | (bytes[dataPos+6] << 16) | (bytes[dataPos+7] << 24);
                    } else {
                        num = (bytes[dataPos] << 24) | (bytes[dataPos+1] << 16) | (bytes[dataPos+2] << 8) | bytes[dataPos+3];
                        den = (bytes[dataPos+4] << 24) | (bytes[dataPos+5] << 16) | (bytes[dataPos+6] << 8) | bytes[dataPos+7];
                    }
                    
                    if (den != 0 && num > 0) {
                        xResolution = Math.round(num / den);
                    }
                }
            }
            
            if (tag == 0x0128 && type == 3 && count == 1) {
                if (byteOrder == "II") {
                    resolutionUnit = bytes[entryOffset+8] | (bytes[entryOffset+9] << 8);
                } else {
                    resolutionUnit = (bytes[entryOffset+8] << 8) | bytes[entryOffset+9];
                }
            }
        }
        
        if (xResolution > 0 && resolutionUnit == 2) {
            dpi = xResolution;
        } else if (xResolution > 0 && resolutionUnit == 3) {
            dpi = Math.round(xResolution * 2.54);
        }
        
        return dpi;
    }
    
    // Функция получения информации о JPEG (быстрый анализ первых байт)
    function getJpegInfoFast(hexData) {
        var info = {
            dpi: -1,
            bitDepth: -1,
            colorModel: "",
            isCMYK: false,
            isGrayscale: false,
            hasJfif: false,
            sofFound: false
        };
        
        if (hexData == "") return info;
        
        var hexParts = hexData.split(" ");
        var bytes = [];
        
        for (var h = 0; h < hexParts.length; h++) {
            if (hexParts[h] != "") {
                bytes.push(parseInt(hexParts[h], 16));
            }
        }
        
        if (bytes.length < 10) return info;
        
        var pos = 2;
        
        while (pos < bytes.length - 10) {
            if (bytes[pos] != 0xFF) {
                pos++;
                continue;
            }
            
            var marker = bytes[pos+1];
            
            if (marker == 0xE0) {
                info.hasJfif = true;
                var jfifStart = pos + 4;
                var units = bytes[jfifStart + 7];
                var xDensity = (bytes[jfifStart + 8] << 8) | bytes[jfifStart + 9];
                
                if (units == 1 && xDensity > 0) {
                    info.dpi = xDensity;
                }
            }
            
            if (marker == 0xE1 && info.dpi == -1) {
                var exifStart = pos + 4;
                var exifDpi = getExifDpi(bytes, exifStart);
                if (exifDpi > 0) {
                    info.dpi = exifDpi;
                }
            }
            
            if (marker == 0xC0 || marker == 0xC2) {
                var precision = bytes[pos+4];
                var numComponents = bytes[pos+9];
                
                info.bitDepth = precision;
                
                if (numComponents == 1) {
                    info.colorModel = "grayscale";
                    info.isGrayscale = true;
                } else if (numComponents == 3) {
                    info.colorModel = "RGB";
                } else if (numComponents == 4) {
                    info.colorModel = "CMYK";
                    info.isCMYK = true;
                }
                
                info.sofFound = true;
                break;
            }
            
            if (marker == 0xD8 || marker == 0xD9) {
                pos += 2;
            } else {
                var segLen = (bytes[pos+2] << 8) | bytes[pos+3];
                pos += 2 + segLen;
            }
        }
        
        return info;
    }
    
    // Функция получения информации о PNG
    function getPngInfo(hexData) {
        var info = {
            dpi: -1,
            bitDepth: -1,
            colorModel: "",
            isCMYK: false,
            isGrayscale: false,
            paletteColors: 0,
            isRGBA: false,
            isGrayAlpha: false
        };
        
        if (hexData == "") return info;
        
        var hexParts = hexData.split(" ");
        var bytes = [];
        
        for (var h = 0; h < hexParts.length; h++) {
            if (hexParts[h] != "") {
                bytes.push(parseInt(hexParts[h], 16));
            }
        }
        
        if (bytes.length < 25) return info;
        
        info.bitDepth = bytes[24];
        var colorType = bytes[25];
        
        if (colorType == 0) {
            info.colorModel = "grayscale";
            info.isGrayscale = true;
        } else if (colorType == 2) {
            info.colorModel = "RGB";
        } else if (colorType == 3) {
            info.colorModel = "Indexed color";
        } else if (colorType == 4) {
            info.colorModel = "grayscale+alpha";
            info.isGrayscale = true;
            info.isGrayAlpha = true;
        } else if (colorType == 6) {
            info.colorModel = "RGBA";
            info.isRGBA = true;
        }
        
        var pos = 8;
        var chunkNum = 0;
        
        while (pos < bytes.length - 12 && chunkNum < 30) {
            var chunkLen = (bytes[pos] << 24) | (bytes[pos+1] << 16) | (bytes[pos+2] << 8) | bytes[pos+3];
            var chunkType = String.fromCharCode(bytes[pos+4]) + String.fromCharCode(bytes[pos+5]) + 
                            String.fromCharCode(bytes[pos+6]) + String.fromCharCode(bytes[pos+7]);
            
            chunkNum++;
            
            if (chunkType == "PLTE") {
                info.paletteColors = Math.floor(chunkLen / 3);
                
                if (info.colorModel == "Indexed color") {
                    var colorWord = getColorWord(info.paletteColors);
                    info.colorModel = "Indexed color " + info.paletteColors + " " + colorWord;
                }
            }
            
            if (chunkType == "pHYs") {
                var xppm = (bytes[pos+8] << 24) | (bytes[pos+9] << 16) | (bytes[pos+10] << 8) | bytes[pos+11];
                var unit = bytes[pos+16];
                
                if (unit == 1) {
                    info.dpi = Math.round(xppm * 0.0254);
                }
                break;
            }
            
            if (chunkType == "IEND") break;
            
            pos += 12 + chunkLen;
        }
        
        return info;
    }
    
    // ==================================================
    // СБОР ДАННЫХ О БИНАРНИКАХ
    // ==================================================
    
    var bin_objects = document.all.binobj.getElementsByTagName("DIV");
    
    if (bin_objects.length == 0) {
        MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\n" +
               "Иллюстраций в документе не обнаружено.");
        return;
    }
    
    var binaries = [];
    var unsupportedFiles = [];
    var excessColorFiles = [];
    var rgbaFiles = [];
    var grayAlphaFiles = [];
    var totalWeight = 0;
    var totalBase64Size = 0;
    
    for (var i = 0; i < bin_objects.length; i++) {
        var bin = bin_objects[i];
        var binInfo = {
            id: "",
            type: "",
            size: 0,
            width: 0,
            height: 0,
            dims: "",
            ext: "",
            dpi: -1,
            bitDepth: -1,
            colorModel: "",
            paletteColors: 0,
            isCMYK: false,
            isGrayscale: false,
            isRGBA: false,
            isGrayAlpha: false,
            isExcessColor: false,
            hasSizeProblem: false,
            hasDpiProblem: false,
            hasColorProblem: false,
            hasExcessColorProblem: false,
            hasLayersProblem: false,
            problemTypes: [],
            isUnsupported: false,
            isUsed: false,
            isCover: false
        };
        
        try {
            binInfo.id = bin.all.id.value;
            binInfo.type = bin.all.type.value;
            binInfo.size = parseInt(bin.all.size.value) || 0;
            binInfo.dims = bin.all.dims.value || "";
        } catch (e) {}
        
        if (binInfo.dims != "") {
            var dimsParts = binInfo.dims.split("x");
            if (dimsParts.length == 2) {
                binInfo.width = parseInt(dimsParts[0]) || 0;
                binInfo.height = parseInt(dimsParts[1]) || 0;
            }
        }
        
        binInfo.ext = getFileExtension(binInfo.id);
        
        if (binInfo.ext != "jpg" && binInfo.ext != "jpeg" && binInfo.ext != "png") {
            binInfo.isUnsupported = true;
            binInfo.problemTypes.push("unsupported");
            unsupportedFiles.push(binInfo);
        }
        
        totalWeight += binInfo.size;
        totalBase64Size += Math.floor(binInfo.size * 4 / 3) + 100;
        
        binaries.push(binInfo);
    }
    
    // ==================================================
    // ПРОВЕРКА РАЗМЕРОВ, DPI, ЦВЕТОВОЙ МОДЕЛИ И ИЗБЫТОЧНОСТИ ЦВЕТА
    // ==================================================
    
    var docFolder = getDocumentPath();
    var docFileName = getDocumentFileName();
    var docFilePath = getDocumentFilePath();
    var problemFiles = [];
    
    for (var b = 0; b < binaries.length; b++) {
        var bin = binaries[b];
        
        if (bin.width > maxWidth || bin.height > maxHeight) {
            bin.hasSizeProblem = true;
            bin.problemTypes.push("size");
        }
        
        if (bin.ext == "jpg" || bin.ext == "jpeg" || bin.ext == "png") {
            var savedPath = "";
            var saveSuccess = false;
            
            try {
                var saveResult = window.external.SaveBinary(bin.id, bin_objects[b].base64data, 0);
                savedPath = docFolder + "\\" + bin.id;
                
                var fsoCheck = new ActiveXObject("Scripting.FileSystemObject");
                if (fsoCheck.FileExists(savedPath)) {
                    saveSuccess = true;
                }
            } catch (e) {}
            
            if (saveSuccess) {
                if (startTime == 0) {
                    startTime = new Date();
                }
                
                var imgInfo = {};
                
                if (bin.ext == "png") {
                    var hexData = getFileHexViaPowerShell(savedPath, 2000);
                    imgInfo = getPngInfo(hexData);
                } else {
                    var hexData = getFileHexViaPowerShell(savedPath, 2000);
                    imgInfo = getJpegInfoFast(hexData);
                    
                    if (!imgInfo.sofFound) {
                        var sofResult = getLastSofViaPowerShell(savedPath);
                        
                        if (sofResult != "") {
                            var sofParts = sofResult.split(":");
                            if (sofParts.length == 3) {
                                imgInfo.bitDepth = parseInt(sofParts[1]);
                                var numComponents = parseInt(sofParts[2]);
                                
                                if (numComponents == 1) {
                                    imgInfo.colorModel = "grayscale";
                                    imgInfo.isGrayscale = true;
                                } else if (numComponents == 3) {
                                    imgInfo.colorModel = "RGB";
                                } else if (numComponents == 4) {
                                    imgInfo.colorModel = "CMYK";
                                    imgInfo.isCMYK = true;
                                }
                            }
                        }
                    }
                }
                
                bin.dpi = imgInfo.dpi;
                bin.bitDepth = imgInfo.bitDepth;
                bin.colorModel = imgInfo.colorModel;
                bin.paletteColors = imgInfo.paletteColors;
                bin.isCMYK = imgInfo.isCMYK;
                bin.isGrayscale = imgInfo.isGrayscale;
                bin.isRGBA = imgInfo.isRGBA;
                bin.isGrayAlpha = imgInfo.isGrayAlpha;
                
                if (bin.isRGBA) {
                    rgbaFiles.push(bin);
                    
                    if (treatLayersAsProblem == 1) {
                        bin.hasLayersProblem = true;
                        bin.problemTypes.push("layers");
                    }
                }
                
                if (bin.isGrayAlpha) {
                    grayAlphaFiles.push(bin);
                    
                    if (treatGrayAlphaAsProblem == 1) {
                        bin.hasLayersProblem = true;
                        bin.problemTypes.push("layers");
                    }
                }
                
                if ((imgInfo.colorModel == "RGB" || imgInfo.colorModel == "RGBA") && !imgInfo.isGrayscale) {
                    var grayCheck = checkActualGrayscale(savedPath);
                    
                    if (grayCheck == "True") {
                        bin.isExcessColor = true;
                        excessColorFiles.push(bin);
                        
                        if (treatExcessColorAsProblem == 1) {
                            bin.hasExcessColorProblem = true;
                            bin.problemTypes.push("excess_color");
                        }
                    }
                }
                
                try {
                    var fsoDel = new ActiveXObject("Scripting.FileSystemObject");
                    if (fsoDel.FileExists(savedPath)) {
                        fsoDel.DeleteFile(savedPath);
                    }
                } catch (e) {}
                
                if (bin.dpi > 0) {
                    if (bin.ext == "jpg" || bin.ext == "jpeg") {
                        if (bin.dpi != standardJpgDpi) {
                            bin.hasDpiProblem = true;
                            bin.problemTypes.push("dpi");
                        }
                    } else if (bin.ext == "png") {
                        if (bin.dpi < standardPngDpiMin || bin.dpi > standardPngDpiMax) {
                            bin.hasDpiProblem = true;
                            bin.problemTypes.push("dpi");
                        }
                    }
                }
                
                if (bin.isCMYK && (bin.ext == "jpg" || bin.ext == "jpeg")) {
                    bin.hasColorProblem = true;
                    bin.problemTypes.push("cmyk");
                }
            }
        }
        
        if (bin.problemTypes.length > 0) {
            problemFiles.push(bin);
        }
    }
    
    if (startTime == 0) {
        startTime = new Date();
    }
    
    // ==================================================
    // ФОРМИРОВАНИЕ СТАТИСТИКИ
    // ==================================================
    
    var totalBinaries = binaries.length;
    var usedBinaries = 0;
    var unusedBinaries = 0;
    var totalImageLinks = 0;
    var uniqueImageLinks = 0;
    var emptyImages = 0;
    var coverFileName = "";
    var usedWeight = 0;
    var unusedWeight = 0;
    var pngCount = 0;
    var jpgCount = 0;
    var otherFormatCounts = {};
    
    try {
        var tiCover = document.getElementById("tiCover");
        if (tiCover) {
            var selects = tiCover.getElementsByTagName("select");
            for (var sc = 0; sc < selects.length; sc++) {
                var selectVal = selects[sc].value || "";
                if (selectVal != "" && selectVal.charAt(0) == "#") {
                    coverFileName = selectVal.substring(1);
                    break;
                }
            }
        }
    } catch (e) {}
    
    var usedFiles = {};
    var allElements = document.getElementsByTagName("*");
    
    for (var e = 0; e < allElements.length; e++) {
        var el = allElements[e];
        var cls = el.className || "";
        
        if (typeof cls == "string" && cls.indexOf("image") != -1) {
            totalImageLinks++;
            var href = el.getAttribute("href") || "";
            
            if (href == "" || href == "#undefined" || href.indexOf("undefined") != -1) {
                emptyImages++;
            } else if (href.charAt(0) == "#") {
                var fileName = href.substring(1);
                usedFiles[fileName] = true;
            }
        }
    }
    
    if (coverFileName != "") {
        usedFiles[coverFileName] = true;
    }
    
    for (var uf2 in usedFiles) {
        if (usedFiles.hasOwnProperty(uf2)) {
            uniqueImageLinks++;
        }
    }
    
    for (var u = 0; u < binaries.length; u++) {
        if (usedFiles[binaries[u].id]) {
            usedBinaries++;
            binaries[u].isUsed = true;
            usedWeight += binaries[u].size;
        } else {
            binaries[u].isUsed = false;
            unusedWeight += binaries[u].size;
        }
        
        if (binaries[u].id == coverFileName) {
            binaries[u].isCover = true;
        }
        
        if (binaries[u].ext == "jpg" || binaries[u].ext == "jpeg") {
            jpgCount++;
        } else if (binaries[u].ext == "png") {
            pngCount++;
        } else if (binaries[u].ext != "") {
            if (!otherFormatCounts[binaries[u].ext]) {
                otherFormatCounts[binaries[u].ext] = 0;
            }
            otherFormatCounts[binaries[u].ext]++;
        }
    }
    
    unusedBinaries = totalBinaries - usedBinaries;
    
    // ==================================================
    // ФОРМИРУЕМ ПОЛНЫЙ ОТЧЕТ
    // ==================================================
    
    var fullReportText = "";
    fullReportText += "ПОЛНЫЙ ОТЧЕТ ПО ВСЕМ БИНАРНЫМ ФАЙЛАМ (" + totalBinaries + "):\r\n";
    fullReportText += "---------------------------------------\r\n";
    
    for (var rb = 0; rb < binaries.length; rb++) {
        var binf = binaries[rb];
        
        var dpiStr = "неизвестно";
        if (binf.dpi > 0) {
            dpiStr = binf.dpi + " dpi";
        }
        
        var sizeStr = formatFileSize(binf.size);
        
        var colorStr = "";
        if (binf.colorModel != "") {
            colorStr = ", " + binf.colorModel;
            if (binf.bitDepth > 0) {
                colorStr += " " + binf.bitDepth + " бит";
            }
        }
        
        if (binf.isExcessColor) {
            colorStr += " (фактически grayscale)";
        }
        
        var statusStr = " OK";
        if (binf.problemTypes.length > 0) {
            statusStr = " PROBLEM";
        }
        
        var prefix = "";
        if (binf.isCover) {
            prefix = "Обложка: ";
        }
        
        var suffix = "";
        if (!binf.isUsed && !binf.isCover) {
            suffix = " (НЕ ИСПОЛЬЗУЕТСЯ)";
        }
        
        fullReportText += prefix + binf.id + " - " + binf.width + "x" + binf.height + " px, " + dpiStr + colorStr + ", " + sizeStr + statusStr + suffix + "\r\n";
    }
    
    if (excessColorFiles.length > 0) {
        fullReportText += "\r\n";
        fullReportText += "========================================\r\n";
        fullReportText += "Бинарные файлы с избыточной цветностью (рекомендуется преобразовать из RGB в grayscale) (" + excessColorFiles.length + ")\r\n";
        fullReportText += "========================================\r\n";
        
        for (var ec = 0; ec < excessColorFiles.length; ec++) {
            var ecf = excessColorFiles[ec];
            
            var dpiStr = "неизвестно";
            if (ecf.dpi > 0) {
                dpiStr = ecf.dpi + " dpi";
            }
            
            var sizeStr = formatFileSize(ecf.size);
            var colorStr = ecf.colorModel;
            if (ecf.bitDepth > 0) {
                colorStr += " " + ecf.bitDepth + " бит";
            }
            
            fullReportText += ecf.id + " - " + ecf.width + "x" + ecf.height + " px, " + dpiStr + ", " + colorStr + ", (фактически grayscale), " + sizeStr + " (избыточная цветность)\r\n";
        }
        
        fullReportText += "========================================\r\n";
    }
    
    if (rgbaFiles.length > 0 || grayAlphaFiles.length > 0) {
        var totalLayerFiles = rgbaFiles.length + grayAlphaFiles.length;
        
        fullReportText += "\r\n";
        fullReportText += "========================================\r\n";
        fullReportText += "PNG со слоями — рекомендуется схлопнуть слои (свести к одному слою) (" + totalLayerFiles + ")\r\n";
        fullReportText += "========================================\r\n";
        
        for (var ra = 0; ra < rgbaFiles.length; ra++) {
            var raf = rgbaFiles[ra];
            
            var dpiStr = "неизвестно";
            if (raf.dpi > 0) {
                dpiStr = raf.dpi + " dpi";
            }
            
            var sizeStr = formatFileSize(raf.size);
            var colorStr = raf.colorModel;
            if (raf.bitDepth > 0) {
                colorStr += " " + raf.bitDepth + " бит";
            }
            
            if (raf.isExcessColor) {
                colorStr += " (фактически grayscale)";
            }
            
            fullReportText += raf.id + " - " + raf.width + "x" + raf.height + " px, " + dpiStr + ", " + colorStr + ", " + sizeStr + " (рекомендуется схлопнуть слои)\r\n";
        }
        
        for (var ga = 0; ga < grayAlphaFiles.length; ga++) {
            var gaf = grayAlphaFiles[ga];
            
            var dpiStr = "неизвестно";
            if (gaf.dpi > 0) {
                dpiStr = gaf.dpi + " dpi";
            }
            
            var sizeStr = formatFileSize(gaf.size);
            var colorStr = gaf.colorModel;
            if (gaf.bitDepth > 0) {
                colorStr += " " + gaf.bitDepth + " бит";
            }
            
            fullReportText += gaf.id + " - " + gaf.width + "x" + gaf.height + " px, " + dpiStr + ", " + colorStr + ", " + sizeStr + " (рекомендуется схлопнуть слои)\r\n";
        }
        
        fullReportText += "========================================\r\n";
    }
    
    // ==================================================
    // ВЫВОД СТАТИСТИКИ ИЛИ ТИХИЙ РЕЖИМ
    // ==================================================
    
    var endTime = new Date();
    var elapsed = 0;
    if (startTime != 0) {
        elapsed = (endTime - startTime) / 1000;
    }
    var elapsedStr = elapsed.toFixed(3).replace(".", ",");
    
    // Переменные для результатов сохранения
    var saveResultInfo = {
        savedCount: 0,
        folderPath: "",
        reportPath: ""
    };
    
    if (showStatistics == 1) {
        // ОБЫЧНЫЙ РЕЖИМ: полная статистика
        var msg = scriptName + "\n";
        msg += "ver. " + version + "\n";
        msg += "---------------------------------------\n\n";
        
        if (docFileName != "") {
            msg += docFileName + "\n\n";
        }
        
        msg += "Всего ссылок на иллюстрации в тексте: " + totalImageLinks + "\n";
        
        if (coverFileName != "") {
            msg += "Обложка - 1 (" + coverFileName + ")\n";
        }
        
        msg += "Уникальных ссылок на иллюстрации: " + uniqueImageLinks + "\n";
        msg += "Пустых иллюстраций: " + emptyImages + "\n";
        
        if (unusedBinaries == 0) {
            msg += "Всего бинарных файлов: " + totalBinaries + " (все используются)\n";
        } else {
            msg += "Всего бинарных файлов: " + totalBinaries + " (используется: " + usedBinaries + ", не используется: " + unusedBinaries + ")\n";
        }
        
        msg += "Всего png: " + pngCount + "\n";
        msg += "Всего jpg: " + jpgCount + "\n";
        
        for (var fmt in otherFormatCounts) {
            if (otherFormatCounts.hasOwnProperty(fmt)) {
                msg += "Всего " + fmt + ": " + otherFormatCounts[fmt] + "\n";
            }
        }
        
        msg += "Общий вес всех бинарных файлов: " + formatFileSize(totalWeight) + "\n";
        msg += "Вес бинарных файлов в FB2 файле (Base64): " + formatFileSize(totalBase64Size) + "\n";
        msg += "Вес используемых бинарных файлов: " + formatFileSize(usedWeight) + "\n";
        
        if (unusedWeight > 0) {
            msg += "Вес неиспользуемых бинарных файлов: " + formatFileSize(unusedWeight) + "\n";
        }
        
        if (unsupportedFiles.length > 0) {
            msg += "\n";
            msg += "Неподдерживаемые форматы: " + unsupportedFiles.length + "\n";
            for (var uf = 0; uf < unsupportedFiles.length && uf < 10; uf++) {
                msg += "  • " + unsupportedFiles[uf].id;
                if (unsupportedFiles[uf].ext != "") {
                    msg += " (" + unsupportedFiles[uf].ext.toUpperCase() + ")";
                }
                if (unsupportedFiles[uf].isUsed) {
                    msg += " — используется";
                } else {
                    msg += " — не используется";
                }
                msg += "\n";
            }
            if (unsupportedFiles.length > 10) {
                msg += "  ... и еще " + (unsupportedFiles.length - 10) + "\n";
            }
        }
        
        if (excessColorFiles.length > 0) {
            msg += "\n";
            msg += "Бинарные файлы с избыточной цветностью: " + excessColorFiles.length + "\n";
            msg += "(рекомендуется преобразовать из RGB в grayscale)\n";
        }
        
        if (rgbaFiles.length > 0 || grayAlphaFiles.length > 0) {
            msg += "\n";
            msg += "PNG со слоями: " + (rgbaFiles.length + grayAlphaFiles.length) + "\n";
            msg += "(рекомендуется схлопнуть слои)\n";
        }
        
        msg += "\n";
        
        if (problemFiles.length == 0) {
            msg += "Бинарных файлов с проблемами нет.\n";
            msg += "Размеры и разрешение иллюстраций в порядке.";
        } else {
            msg += "Обнаружены проблемные бинарные файлы:\n";
            msg += "---------------------------------------\n";
            
            var showCount = Math.min(problemFiles.length, maxShowInMsgBox);
            
            for (var p = 0; p < showCount; p++) {
                var pf = problemFiles[p];
                var problems = [];
                
                if (pf.hasSizeProblem) problems.push("следует уменьшить размеры");
                if (pf.hasDpiProblem) problems.push("следует изменить разрешение");
                if (pf.hasColorProblem && pf.isCMYK) problems.push("конвертировать в RGB");
                if (pf.isUnsupported) problems.push("неподдерживаемый формат");
                if (pf.hasExcessColorProblem) problems.push("избыточная цветность");
                if (pf.hasLayersProblem) problems.push("рекомендуется схлопнуть слои");
                
                var problemDesc = problems.join(" и ");
                
                var dpiStr = (pf.dpi > 0) ? pf.dpi + " dpi" : "dpi неизвестно";
                var sizeStr = formatFileSize(pf.size);
                var colorStr = (pf.colorModel != "") ? pf.colorModel : "";
                if (pf.isExcessColor) {
                    colorStr += " (фактически grayscale)";
                }
                
                msg += pf.id + " - " + pf.width + "x" + pf.height + " px, " + dpiStr + ", " + colorStr + ", " + sizeStr + " (" + problemDesc + ")\n";
            }
            
            if (problemFiles.length > maxShowInMsgBox) {
                msg += "\n... и еще " + (problemFiles.length - maxShowInMsgBox) + " файлов\n";
            }
            
            msg += "\n---------------------------------------\n";
            msg += "Всего проблемных файлов: " + problemFiles.length + "\n";
        }
        
        if (showFullReport == 1) {
            msg += "\n\n" + fullReportText;
        }
        
        msg += "\n---------------------------------------\n";
        msg += "Время выполнения: " + elapsedStr + " сек.";
        
        MsgBox(msg);
        
        // Спрашиваем про сохранение
        if ((alwaysWriteReport == 1 || problemFiles.length > 0) && docFolder != "") {
            var saveMsg = scriptName + "\nver. " + version + "\n---------------------------------------\n\n";
            
            if (problemFiles.length > 0) {
                saveMsg += "Сохранить проблемные бинарные файлы и отчет в папку рядом с документом?";
            } else {
                saveMsg += "Сохранить отчет о проверке иллюстраций в папку рядом с документом?";
            }
            
            if (AskYesNo(saveMsg)) {
                saveResultInfo = saveProblemFiles(problemFiles, bin_objects, docFolder, docFileName, docFilePath, scriptName, version, fullReportText, totalWeight, totalBase64Size, usedWeight, unusedWeight, totalBinaries, usedBinaries, unusedBinaries, pngCount, jpgCount, otherFormatCounts);
            }
        }
    } else {
        // ТИХИЙ РЕЖИМ: автоматически сохраняем, показываем краткое сообщение
        if ((alwaysWriteReport == 1 || problemFiles.length > 0) && docFolder != "") {
            saveResultInfo = saveProblemFiles(problemFiles, bin_objects, docFolder, docFileName, docFilePath, scriptName, version, fullReportText, totalWeight, totalBase64Size, usedWeight, unusedWeight, totalBinaries, usedBinaries, unusedBinaries, pngCount, jpgCount, otherFormatCounts);
        }
        
        var quietMsg = scriptName + "\n";
        quietMsg += "ver. " + version + "\n";
        quietMsg += "---------------------------------------\n\n";
        quietMsg += "Проверка завершена.\n";
        
        if (problemFiles.length == 0) {
            quietMsg += "Проблемных бинарных файлов не обнаружено.\n";
        } else {
            quietMsg += "Обнаружено проблемных бинарных файлов: " + problemFiles.length + "\n";
        }
        
        if (saveResultInfo.reportPath != "") {
            quietMsg += "Отчет записан: " + saveResultInfo.reportPath + "\n";
        }
        
        if (saveResultInfo.savedCount > 0) {
            quietMsg += "Сохранено файлов: " + saveResultInfo.savedCount + "\n";
        }
        
        quietMsg += "\n---------------------------------------\n";
        quietMsg += "Время выполнения: " + elapsedStr + " сек.";
        
        MsgBox(quietMsg);
    }
}

// Функция сохранения проблемных файлов и отчёта
function saveProblemFiles(problemFiles, bin_objects, docFolder, docFileName, docFilePath, scriptName, version, fullReportText, totalWeight, totalBase64Size, usedWeight, unusedWeight, totalBinaries, usedBinaries, unusedBinaries, pngCount, jpgCount, otherFormatCounts) {
    var result = {
        savedCount: 0,
        folderPath: "",
        reportPath: ""
    };
    
    try {
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        
        for (var i = 0; i < problemFiles.length; i++) {
            var pf = problemFiles[i];
            var existingFile = docFolder + "\\" + pf.id;
            
            if (fso.FileExists(existingFile)) {
                try {
                    fso.DeleteFile(existingFile);
                } catch (e) {}
            }
        }
        
        var folderName = "Problem_images";
        var folderPath = docFolder + "\\" + folderName;
        var counter = 1;
        
        while (fso.FolderExists(folderPath)) {
            folderName = "Problem_images_" + counter;
            folderPath = docFolder + "\\" + folderName;
            counter++;
        }
        
        fso.CreateFolder(folderPath);
        result.folderPath = folderPath;
        
        var savedCount = 0;
        
        for (var i = 0; i < problemFiles.length; i++) {
            var pf = problemFiles[i];
            
            for (var b = 0; b < bin_objects.length; b++) {
                var binId = "";
                try {
                    binId = bin_objects[b].all.id.value;
                } catch (e) {}
                
                if (binId == pf.id) {
                    try {
                        var saveResult = window.external.SaveBinary(pf.id, bin_objects[b].base64data, 0);
                        
                        var savedPath1 = docFolder + "\\" + pf.id;
                        var savedPath2 = folderPath + "\\" + pf.id;
                        
                        if (fso.FileExists(savedPath1)) {
                            fso.MoveFile(savedPath1, savedPath2);
                            savedCount++;
                        } else if (fso.FileExists(savedPath2)) {
                            savedCount++;
                        }
                    } catch (e) {}
                    break;
                }
            }
        }
        
        result.savedCount = savedCount;
        
        var now = new Date();
        var day = now.getDate();
        var month = now.getMonth() + 1;
        var year = now.getFullYear();
        
        if (day < 10) day = "0" + day;
        if (month < 10) month = "0" + month;
        
        var dateStr = day + "-" + month + "-" + year;
        var reportFileName = "Отчет по качеству иллюстраций_" + dateStr + ".txt";
        var reportPath = folderPath + "\\" + reportFileName;
        var reportFile = fso.CreateTextFile(reportPath, true, true);
        
        reportFile.Write("========================================\r\n");
        reportFile.Write("ОТЧЕТ О ПРОВЕРКЕ ИЛЛЮСТРАЦИЙ\r\n");
        reportFile.Write("Скрипт \"" + scriptName + "\"\r\n");
        reportFile.Write("ver. " + version + "\r\n");
        reportFile.Write("========================================\r\n");
        reportFile.Write("Дата: " + now.toLocaleDateString() + " " + now.toLocaleTimeString() + "\r\n");
        reportFile.Write("\r\n");
        reportFile.Write("fb2 документ:\r\n");
        
        if (docFilePath != "") {
            reportFile.Write("Путь: " + docFilePath + "\r\n");
        }
        
        if (docFileName != "") {
            reportFile.Write("Имя файла: " + docFileName + "\r\n");
        }
        
        reportFile.Write("========================================\r\n");
        reportFile.Write("\r\n");
        
        if (unusedBinaries == 0) {
            reportFile.Write("Всего бинарных файлов: " + totalBinaries + " (все используются)\r\n");
        } else {
            reportFile.Write("Всего бинарных файлов: " + totalBinaries + " (используется: " + usedBinaries + ", не используется: " + unusedBinaries + ")\r\n");
        }
        
        reportFile.Write("Всего png: " + pngCount + "\r\n");
        reportFile.Write("Всего jpg: " + jpgCount + "\r\n");
        
        for (var fmt in otherFormatCounts) {
            if (otherFormatCounts.hasOwnProperty(fmt)) {
                reportFile.Write("Всего " + fmt + ": " + otherFormatCounts[fmt] + "\r\n");
            }
        }
        
        reportFile.Write("Общий вес всех бинарных файлов: " + formatFileSize(totalWeight) + "\r\n");
        reportFile.Write("Вес бинарных файлов в FB2 файле (Base64): " + formatFileSize(totalBase64Size) + "\r\n");
        reportFile.Write("Вес используемых бинарных файлов: " + formatFileSize(usedWeight) + "\r\n");
        
        if (unusedWeight > 0) {
            reportFile.Write("Вес неиспользуемых бинарных файлов: " + formatFileSize(unusedWeight) + "\r\n");
        }
        
        reportFile.Write("========================================\r\n");
        reportFile.Write("\r\n");
        
        if (problemFiles.length > 0) {
            reportFile.Write("Список проблемных бинарных файлов (" + problemFiles.length + "):\r\n");
            reportFile.Write("========================================\r\n");
            
            for (var p = 0; p < problemFiles.length; p++) {
                var pf = problemFiles[p];
                var problems = [];
                
                if (pf.hasSizeProblem) problems.push("следует уменьшить размеры");
                if (pf.hasDpiProblem) problems.push("следует изменить разрешение");
                if (pf.hasColorProblem && pf.isCMYK) problems.push("конвертировать в RGB");
                if (pf.isUnsupported) problems.push("неподдерживаемый формат");
                if (pf.hasExcessColorProblem) problems.push("избыточная цветность");
                if (pf.hasLayersProblem) problems.push("рекомендуется схлопнуть слои");
                
                var problemDesc = problems.join(" и ");
                var dpiStr = (pf.dpi > 0) ? pf.dpi + " dpi" : "dpi неизвестно";
                var sizeStr = formatFileSize(pf.size);
                var colorStr = (pf.colorModel != "") ? pf.colorModel : "";
                if (pf.isExcessColor) {
                    colorStr += " (фактически grayscale)";
                }
                
                reportFile.Write(pf.id + " - " + pf.width + "x" + pf.height + " px, " + dpiStr + ", " + colorStr + ", " + sizeStr + " (" + problemDesc + ")\r\n");
            }
            
            reportFile.Write("========================================\r\n");
            reportFile.Write("\r\n");
        }
        
        reportFile.Write(fullReportText);
        reportFile.Write("\r\n");
        reportFile.Write("========================================\r\n");
        
        if (problemFiles.length > 0) {
            reportFile.Write("Всего проблемных файлов: " + problemFiles.length + "\r\n");
            reportFile.Write("Сохранено: " + savedCount + "\r\n");
        }
        
        reportFile.Write("Папка: " + folderPath + "\r\n");
        reportFile.Write("========================================\r\n");
        reportFile.Close();
        
        result.reportPath = reportPath;
        
    } catch (e) {}
    
    return result;
}