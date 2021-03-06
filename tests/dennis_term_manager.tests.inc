<?php
/**
 * @file
 * Testing suite for Dennis Term Manager.
 */

/**
 * Run CSV to create the initial tree.
 */
function dennis_term_manager_tests_create() {
  $destination = _dennis_term_manager_get_files_folder();
  $actions_file = drupal_get_path('module', 'dennis_term_manager') . '/tests/test_create_run.csv';

  // Initial cleanup of taxonomy tree.
  _dennis_term_manager_cleanup();

  // Copy the CSV file into files folder.
  $file = dennis_term_manager_file_copy($actions_file, $destination);

  // Process file.
  dennis_term_manager_test_process($file);
}

/**
 * Compare with the created tree vs expected tree.
 */
function dennis_term_manager_tests_test_create() {
  // Export CSV of taxonomy tree.
  $columns = dennis_term_manager_default_columns();

  // Need to exclude some columns because they will be different on each site.
  $exclude = array('path', 'tid', 'target_tid');
  foreach ($exclude as $item) {
    unset ($columns[array_search($item, $columns)]);
  }
  dennis_term_manager_export_terms(',', array('Category'), $columns, DENNIS_TERM_MANAGER_DESTINATION_FILE);

  $destination = _dennis_term_manager_get_files_folder();
  $exported_tree = drupal_realpath($destination) . '/taxonomy_export.csv';

  // The pass tree is stored in the repo and used for the comparison with generated tree.
  $pass_tree = drupal_get_path('module', 'dennis_term_manager') . '/tests/test_create_pass.csv';

  // Compare exported CSV against the CSV saved on the repo.
  // Pass tree must be contained inside the exported tree in order for the test to pass.
  dennis_term_manager_diff($pass_tree, $exported_tree);
}

/**
 * Run CSV with actions.
 */
function dennis_term_manager_tests_actions() {
  $destination = _dennis_term_manager_get_files_folder();
  $actions_file = drupal_get_path('module', 'dennis_term_manager') . '/tests/test_actions_run.csv';

  // Copy the file into files folder.
  $file = dennis_term_manager_file_copy($actions_file, $destination);

  // Process file.
  dennis_term_manager_test_process($file);
}

/**
 * Test actions against the expected resulting tree.
 */
function dennis_term_manager_tests_test_actions() {
  // Export CSV of taxonomy tree.
  $columns = dennis_term_manager_default_columns();

  // Need to exclude some columns because they will be different on each site.
  $exclude = array('path', 'tid', 'target_tid');
  foreach ($exclude as $item) {
    unset ($columns[array_search($item, $columns)]);
  }
  dennis_term_manager_export_terms(',', array('Category'), $columns, DENNIS_TERM_MANAGER_DESTINATION_FILE);

  $destination = _dennis_term_manager_get_files_folder();

  // Get the exported terms.
  $exported_tree = drupal_realpath($destination) . '/taxonomy_export.csv';

  // Pass tree is stored in the repo
  $pass_tree = drupal_get_path('module', 'dennis_term_manager') . '/tests/test_actions_pass.csv';

  // Compare exported CSV against the CSV saved on the repo.
  // Pass tree must be contained inside the exported tree in order for the test to pass.
  dennis_term_manager_diff($pass_tree, $exported_tree);
}

/**
 * Runs actions with duplicated terms, using the tid column.
 * This function will find duplicated term names and create a CSV file with actions to merge them
 * i.e. Raspberry-0 will be merged to Raspberry.
 */
function dennis_term_manager_tests_dupe_actions() {
  $test_actions = array('merge', 'move parent');

  // Updated duplicated names, by removing the '-0' suffix.
  // This way we will end up with the same term name more than once. Useful to test the actions using tids.
  if (!$result = db_query("UPDATE {taxonomy_term_data} SET name = REPLACE(name, '-0', '') WHERE name like 'TM-%-0'")) {
    throw new Exception(t('Could not find/rename any term.'));
  }

  // Export tree.
  dennis_term_manager_export_terms(',', array('Category'), array(), DENNIS_TERM_MANAGER_DESTINATION_FILE);

  // Load the exported tree.
  $destination = _dennis_term_manager_get_files_folder();
  $exported_tree = drupal_realpath($destination) . '/taxonomy_export.csv';

  // Loop the CSV and add "merge" action to each duplicated term.
  $processed = array();
  $actions = array();
  if (($handle = fopen($exported_tree, "r")) !== FALSE) {
    $delimiter = _dennis_term_manager_detect_delimiter(file_get_contents($exported_tree));
    $heading_row = fgetcsv($handle, 1000, $delimiter);
    $columns = array_flip($heading_row);
    $vocabulary_name_column = $columns['vocabulary_name'];
    $name_column = $columns['term_name'];
    $tid_column = $columns['tid'];
    $target_tid_column = $columns['target_tid'];
    $target_term_name_column = $columns['target_term_name'];
    $target_vocabulary_name_column = $columns['target_vocabulary_name'];
    $action_column = $columns['action'];
    $term_child_count_column = $columns['term_child_count'];

    $row = 0;
    while (($data = fgetcsv($handle, 1000, $delimiter)) !== FALSE) {
      $vocabulary_name = $data[$vocabulary_name_column];
      $term_name = $data[$name_column];
      $tid = $data[$tid_column];

      if (!isset($processed[$term_name])) {
        // Store tid.
        $processed[$term_name] = $tid;
      }
      else {
        // Create action for duplicated term.
        $data[$action_column] = $test_actions[$row];
        $data[$target_tid_column] = $processed[$term_name];
        $data[$target_term_name_column] = $term_name;
        $data[$target_vocabulary_name_column] = $vocabulary_name;
        $actions[] = $data;

        // This counter is used to alternate the actions that are dynamically created.
        $row++;
        if ($row >= count($test_actions)) {
          $row = 0;
        }
      }
    }
  }
  // Sort actions by term_child_count, to make sure we process the children first.
  global $dennis_term_manager_sbk;
  $dennis_term_manager_sbk = $term_child_count_column;
  uasort($actions, '_dennis_term_manager_sbk');

  $out_filename = '/tmp/dupe_actions.csv';

  // Create new csv with actions.
  $out = fopen($out_filename, 'w');
  fputcsv($out, $heading_row, $delimiter, '"');
  foreach ($actions as $action) {
    fputcsv($out, $action, $delimiter, '"');
  }

  // Process file.
  $file = dennis_term_manager_file_copy($out_filename, $destination);

  dennis_term_manager_test_process($file);
}

/**
 * Test actions against the expected resulting tree. This test will only work
 * if you call tm-test-dupe-actions straight after tm-actions command.
 */
function dennis_term_manager_tests_test_dupe_actions() {
  // Export CSV of taxonomy tree.
  $columns = dennis_term_manager_default_columns();

  // Need to exclude some columns because they will be different on each site.
  $exclude = array('path', 'tid', 'target_tid');
  foreach ($exclude as $item) {
    unset ($columns[array_search($item, $columns)]);
  }
  dennis_term_manager_export_terms(',', array('Category'), $columns, DENNIS_TERM_MANAGER_DESTINATION_FILE);

  $destination = _dennis_term_manager_get_files_folder();

  // Get the exported terms.
  $exported_tree = drupal_realpath($destination) . '/taxonomy_export.csv';

  // Pass tree is stored in the repo
  $pass_tree = drupal_get_path('module', 'dennis_term_manager') . '/tests/test_dupe_actions_pass.csv';

  // Compare exported CSV against the CSV saved on the repo.
  // Pass tree must be contained inside the exported tree in order for the test to pass.
  dennis_term_manager_diff($pass_tree, $exported_tree);
}

/**
 * Process the file
 */
function dennis_term_manager_test_process($file) {
  if (!is_object($file)) {
    return;
  }

  $batch = _dennis_term_manager_batch_init($file);
  batch_set($batch);

  $batch =& batch_get();
  if (!empty($batch)) {
    $batch['progressive'] = FALSE;

    drush_backend_batch_process();

    // Process the queue.
    foreach (dennis_term_manager_cron_queue_info() as $queue_name => $info) {
      $function = $info['worker callback'];
      if ($queue = DrupalQueue::get($queue_name)) {
        while ($item = $queue->claimItem()) {
          $function($item->data);
          $queue->deleteItem($item);
        }
      }
    }
  }

  $date = date('Y-m-d_H-i-s', REQUEST_TIME);
  $errors_file = preg_replace("/(.*)[.](.*)/", "$1-$date-errors.$2", $file->uri);
  $dry_run_file = preg_replace("/(.*)[.](.*)/", "$1-$date-dry_run.$2", $file->uri);
  $report_file = preg_replace("/(.*)[.](.*)/", "$1-$date-report.txt", $file->uri);

  // Test that file with errors doesn't exist.
  if (file_exists($errors_file)) {
    throw new Exception(t('There were errors during execution, see !file_name for more details', array(
      '!file_name' => $errors_file,
    )));
  }
}

/**
 * Helper to do a Diff between files.
 * @todo: Rename this function:
 * Initially this function would do a diff,
 * but now it just tests if the term tree matches
 * the terms created.
 */
function dennis_term_manager_diff($pass_tree, $exported_tree) {
  if (!file_exists($pass_tree)) {
    throw new Exception(t('!file doesn\'t exist', array(
      '!file' => $pass_tree,
    )));
  }
  $test_content = file_get_contents($pass_tree);

  if (!file_exists($exported_tree)) {
    throw new Exception(t('!file doesn\'t exist', array(
      '!file' => $exported_tree,
    )));
  }
  $tree_content = file_get_contents($exported_tree);

  // Remove heading.
  $test_content_lines = explode("\n", $test_content);
  array_shift($test_content_lines);
  $test_content = implode("\n", $test_content_lines);

  // Check if the pass tree is in the exported tree.
  if (strpos($tree_content, $test_content, 0) === FALSE) {
    // Get the failing line.
    $test_content_cumulative = '';
    foreach ($test_content_lines as $line) {
      $test_content_cumulative .= $line . "\n";
      if (strpos($tree_content, $test_content_cumulative, 0) === FALSE) {
        $failing_line = $line;
        break;
      }
    }
    // Throw exception with failing line.
    throw new Exception(t('Exported tree !file1 doesn\'t match !file2 at row !line', array(
      '!file1' => $exported_tree,
      '!file2' => $pass_tree,
      '!line' => $failing_line,
    )));
  }
}

/**
 * Helper to clean up after tests run.
 */
function _dennis_term_manager_cleanup() {
  // Delete terms created during tests.
  $term = reset(taxonomy_get_term_by_name('Temp', 'category'));
  taxonomy_term_delete($term->tid);
  $term = reset(taxonomy_get_term_by_name('TM-Fruits', 'category'));
  taxonomy_term_delete($term->tid);
  $term = reset(taxonomy_get_term_by_name('TM-Fruits2', 'category'));
  taxonomy_term_delete($term->tid);

  // Make sure term manager is enabled.
  variable_set('dennis_term_manager_enabled', 1);

  // Clean up queue.
  db_delete('queue')
    ->condition('name', 'dennis_term_manager_queue')
    ->execute();
}
