<?php

function mukurtu_export_form($form, &$form_state) {
  $export_ids = isset($form_state['mukurtu_export']['export_ids']) ? $form_state['mukurtu_export']['export_ids'] : NULL;
  $formatter = isset($form_state['mukurtu_export']['formatter']) ? $form_state['mukurtu_export']['formatter'] : NULL;
  $formatter_settings = isset($form_state['mukurtu_export']['formatter_settings']) ? $form_state['mukurtu_export']['formatter_settings'] : NULL;
  $field_list = isset($form_state['mukurtu_export']['export_fields']) ? $form_state['mukurtu_export']['export_fields'] : NULL;

  if(is_null($export_ids)){// && isset($form_state['build_info']['args'][0])) {
    //kpr("menu");
    //kpr(menu_get_object());
    $current_node = menu_get_object();
    $export_ids = array('node' => array($current_node->type => array($current_node->nid => $current_node)));
  }

  // No format selected
  $format_handlers = mukurtu_export_format_handlers();
  // Format not specified or specified format isn't available
  if(is_null($formatter) || !in_array($formatter, array_keys($format_handlers))) {
    /*// Check if the user selected a format previously
    if(isset($_SESSION['mukurtu_export']['format'])
              && in_array($_SESSION['mukurtu_export']['format'], array_keys($format_handlers))) {
      $formatter = $_SESSION['mukurtu_export']['format'];
    } else { */
      // Only one available format, default to that
      if(count($format_handlers) == 1) {
        $formatter = array_keys($format_handlers)[0];
        $form_state['mukurtu_export']['formatter'] = $formatter;
      } else {
        // Multiple formats, show selection form
        return mukurtu_export_format_selection_form($form, $form_state, $format_handlers);
      }
//    }
  }

  //include_once "formats/csv.inc";
  include_once $format_handlers[$formatter]['#file'];


  // Format selected, need formatter settings
  if(is_null($formatter_settings)) {
    // Get settings form for formatter
    if(isset($format_handlers[$formatter]['#settings_callback']) && function_exists($format_handlers[$formatter]['#settings_callback'])) {
      $form['export']['settings'] = call_user_func($format_handlers[$formatter]['#settings_callback'], $form, $form_state);
    }
    $form['export_next_step'] = array(
      '#type' => 'submit',
      '#value' => t('Next'),
    );
    return $form;
  }

  if(is_null($field_list)) {

    $form['download_file_top'] = array(
      '#type' => 'submit',
      '#value' => t('Download'),
    );

    $form = mukurtu_export_fields_selection_form($form, $form_state, $export_ids);

    $form['download_file_bottom'] = array(
      '#type' => 'submit',
      '#value' => t('Download'),
    );
    return $form;
  }

    //$code_string = mukurtu_export_csv_export($export_ids, $formatter, $field_list);
  $download_file = call_user_func($format_handlers[$formatter]['#export_callback'], $export_ids, $field_list, $formatter_settings);
//kpr($download_file);

  mukurtu_export_get_file($download_file);
}

function mukurtu_export_get_file($file) {
  header('Content-type: ' . $file->filemime);
  header('Content-Disposition: attachment; filename="' . $file->filename . '"');
  print(file_get_contents(drupal_realpath($file->uri)));
  file_delete($file);
  module_invoke_all('exit');
  exit;
}



function mukurtu_export_form_submit($form, &$form_state) {
  //$export_ids = NULL;
  //$code_string = NULL;
  //$field_list = isset($form_state['values']['export_fields']) ? $form_state['values']['export_fields'] : 1;
  //$_SESSION['mukurtu_export']['field_list'] = $field_list;

  // Content to export
  $form_state["rebuild"] = TRUE;
  if(isset($form_state['values']['export_ids'])) {
    $form_state['mukurtu_export']['export_ids'] = $form_state['values']['export_ids'];
  }

  // Formatter
  if(isset($form_state['values']['export_format'])) {
    $form_state['mukurtu_export']['formatter'] = $form_state['values']['export_format'];
  }

  // Formatter Settings
  if(isset($form_state['mukurtu_export']['formatter'])) {
    $format = $form_state['mukurtu_export']['formatter'];
    if(isset($form_state['values'][$format])) {
      $form_state['mukurtu_export']['formatter_settings'] = $form_state['values'][$format];
    }
  }

  // Field List
  if(isset($form_state['values']['export_fields'])) {
    $form_state['mukurtu_export']['export_fields'] = $form_state['values']['export_fields'];
    $_SESSION['mukurtu_export']['export_fields'] = $form_state['values']['export_fields'];
  }
  //return drupal_get_form('mukurtu_export_form', $export_ids, $code_string, 'csv', $field_list);
}

function mukurtu_export_format_selection_form($form, &$form_state, $format_handlers, $default_format = 'csv') {
  $formatter_options = array();
  foreach($format_handlers as $format => $format_info) {
    $formatter_options[$format] = $format_info['#title'];
  }

  $form['export_format'] = array(
    '#type' => 'radios',
    '#title' => t('Export Format'),
    '#options' => $formatter_options,
    '#default_value' => $default_format,
    '#description' => t('Select the format for export.'),
    '#tree' => TRUE,
  );

  $form['export_next_step'] = array(
    '#type' => 'submit',
    '#value' => t('Next'),
  );

  return $form;
}

function mukurtu_export_format_selection_form_submit($form, &$form_state) {
  $_SESSION['mukurtu_export']['format'] = $form_state['values']['export_format'];
}

function mukurtu_export_fields_selection_form($form, &$form_state, $export_ids = NULL, $reset = FALSE) {
  if(!is_null($export_ids)) {
    $form['export_fields'] = array(
      '#type' => 'fieldset',
      '#title' => t('Select Fields for Export'),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
      '#tree' => TRUE,
    );

    // Load defaults
    $defaults = NULL;
    if(!$reset && isset($_SESSION['mukurtu_export']['export_fields'])) {
      $defaults = $_SESSION['mukurtu_export']['export_fields'];
    }

    foreach(array_keys(entity_get_info()) as $entity_type) {

      // Currently only show node/scald_atom fields
      if($entity_type != 'node' && $entity_type != 'scald_atom'){
        continue;
      }

      $entity_info = entity_get_info($entity_type);
      $id = _mukurtu_export_entity_id($entity_type);

      foreach($entity_info['bundles'] as $bundle_name => $ids) {
        $form['export_fields'][$entity_type][$bundle_name] = array(
          '#type' => 'fieldset',
          '#title' => $entity_info['label'] . ' - ' . $entity_info['bundles'][$bundle_name]['label'],
          '#collapsible' => TRUE,
          '#collapsed' => TRUE,
          '#weight' => 0,
        );

        $fields_info = field_info_instances($entity_type, $bundle_name);

        // Default fields
        // ID
        $id_label = t(strtoupper($id));

        $form['export_fields'][$entity_type][$bundle_name][$id] = array(
          '#type' => 'checkbox',
          '#title' => $id_label,
          '#default_value' => isset($defaults[$entity_type][$bundle_name][$id]) ? $defaults[$entity_type][$bundle_name][$id] : TRUE,
          '#weight' => -2
        );

        // Title
        $form['export_fields'][$entity_type][$bundle_name]['title'] = array(
          '#type' => 'checkbox',
          '#title' => t('Title'),
          '#default_value' => isset($defaults[$entity_type][$bundle_name]['title']) ? $defaults[$entity_type][$bundle_name]['title'] : TRUE,
          '#weight' => -1
        );

        foreach($fields_info as $field_name => $field) {
          $form['export_fields'][$entity_type][$bundle_name][$field_name] = array(
            '#type' => 'checkbox',
            '#title' => $field['label'],
            '#default_value' => isset($defaults[$entity_type][$bundle_name][$field_name]) ? $defaults[$entity_type][$bundle_name][$field_name] : TRUE,
            '#weight' => $field['widget']['weight']
          );
        }
      }
    }

    // Re-order form based on content types we are exporting
    $entity_weight = -1000;
    $bundle_weight = -1000;
    foreach($export_ids as $entity_type => $bundles) {
      $form['export_fields'][$entity_type]['#weight'] = $entity_weight++;
      foreach($bundles as $bundle_name => $ids) {
        $form['export_fields'][$entity_type][$bundle_name]['#weight'] = $bundle_weight++;
        $form['export_fields'][$entity_type][$bundle_name]['#collapsed'] = FALSE;
      }
    }

  }
  return $form;
}