// -----------------------------------------------------------------------------
// File system builder
// -----------------------------------------------------------------------------
 
const gulp = require('gulp');
const plumber = require('gulp-plumber');
const htmlmin = require('gulp-htmlmin');
const cleancss = require('gulp-clean-css');
const uglify = require('gulp-uglify');
const gzip = require('gulp-gzip');
const del = require('del');
const useref = require('gulp-useref');
const gulpif = require('gulp-if');
 
/* Clean destination folder */
gulp.task('clean', function() {
    return del(['data/*']);
});
 
/* Copy static files */
gulp.task('files', function() {
    return gulp.src([
            'html/**/*.{jpg,jpeg,png,ico,gif}',
            'html/fsversion'
        ])
        .pipe(gulp.dest('data/'));
});

/* copy fonts */
gulp.task('fonts',function(){
	return gulp.src([
		'html/**/*.{svg,eot,otf}',
		'html/fsversion'
	])
	.pipe(gzip())
	.pipe(gulp.dest('data/'));
});

gulp.task('copy', function(){
	return gulp.src([
		'html/**/*.{woff,ttf}',
		'html/fsversion'
	])
	.pipe(gulp.dest('data/'));
});

gulp.task('copytxt', function(){
	return gulp.src([
		'html/*.txt',
		'html/fsversion'
	])
	.pipe(gulp.dest('data'));
});

gulp.task('copyhtml',function(){
	return gulp.src([
		'html/*.html',
		'html/fsversion'
	])
	.pipe(gzip())
	.pipe(gulp.dest('./data/'));
});


/* Process HTML, CSS, JS */
gulp.task('copyall', function() {
    return gulp.src(['html/*'])
        .pipe(useref())
        .pipe(plumber())
        .pipe(gulpif('*.css', cleancss()))
        .pipe(gulpif('*.js', uglify()))
        .pipe(gulpif('*.htm', htmlmin({
            collapseWhitespace: true,
            removeComments: true,
            minifyCSS: true,
            minifyJS: true
        })))
        .pipe(gzip())
        .pipe(gulp.dest("data"));
});

/* Build file system */
gulp.task('buildfs', ['copyall']);//['clean', 'copyhtml', 'copytxt']);//['clean', 'files', 'fonts', 'copy', 'html']);
gulp.task('default', ['buildfs']);
 
// -----------------------------------------------------------------------------
// PlatformIO support
// -----------------------------------------------------------------------------
 
const spawn = require('child_process').spawn;
const argv = require('yargs').argv;
 
var platformio = function(target) {
    var args = ['run'];
    if ("e" in argv) { args.push('-e'); args.push(argv.e); }
    if ("p" in argv) { args.push('--upload-port'); args.push(argv.p); }
    if (target) { args.push('-t'); args.push(target); }
    const cmd = spawn('platformio', args);
    cmd.stdout.on('data', function(data) { console.log(data.toString().trim()); });
    cmd.stderr.on('data', function(data) { console.log(data.toString().trim()); });
}
 
gulp.task('uploadfs', ['buildfs'], function() { platformio('uploadfs'); });
gulp.task('upload', function() { platformio('upload'); });
gulp.task('run', function() { platformio(false); });